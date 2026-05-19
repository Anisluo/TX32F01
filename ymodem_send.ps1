param(
    [Parameter(Mandatory=$true)] [string]$ComPort,
    [Parameter(Mandatory=$true)] [string]$BinPath,
    [int]$Baud = 115200,
    [int]$TimeoutSec = 60
)

$ErrorActionPreference = 'Stop'

# --- helpers ---
$SOH = 0x01; $STX = 0x02; $EOT = 0x04; $ACK = 0x06; $NAK = 0x15; $CAN = 0x18; $CCH = 0x43

function CRC16-Xmodem([byte[]]$data, [int]$len) {
    $crc = 0
    for ($i = 0; $i -lt $len; $i++) {
        $crc = $crc -bxor ([int]$data[$i] -shl 8)
        for ($j = 0; $j -lt 8; $j++) {
            if (($crc -band 0x8000) -ne 0) { $crc = (($crc -shl 1) -bxor 0x1021) -band 0xFFFF }
            else                            { $crc =  ($crc -shl 1)               -band 0xFFFF }
        }
    }
    return $crc
}

function Write-Pkt([System.IO.Ports.SerialPort]$port, [byte]$hdr, [byte]$seq, [byte[]]$payload) {
    $hdrBytes = @($hdr, $seq, [byte](0xFF - $seq))
    $crc = CRC16-Xmodem $payload $payload.Length
    $crcBytes = @([byte](($crc -shr 8) -band 0xFF), [byte]($crc -band 0xFF))
    $port.Write([byte[]]$hdrBytes, 0, 3)
    $port.Write($payload, 0, $payload.Length)
    $port.Write([byte[]]$crcBytes, 0, 2)
}

function Read-One([System.IO.Ports.SerialPort]$port, [int]$timeoutMs) {
    $deadline = (Get-Date).AddMilliseconds($timeoutMs)
    $orig = $port.ReadTimeout
    $port.ReadTimeout = 100
    while ((Get-Date) -lt $deadline) {
        try { $b = $port.ReadByte(); $port.ReadTimeout = $orig; return $b } catch { }
    }
    $port.ReadTimeout = $orig
    return -1
}

function Wait-For([System.IO.Ports.SerialPort]$port, [int]$expected, [int]$timeoutMs, [string]$label) {
    $deadline = (Get-Date).AddMilliseconds($timeoutMs)
    while ((Get-Date) -lt $deadline) {
        $b = Read-One $port 500
        if ($b -lt 0) { continue }
        if ($b -eq $expected) {
            Write-Host ("  <- " + $label + " (0x{0:X2})" -f $b) -ForegroundColor Green
            return $true
        } else {
            if ($b -ge 0x20 -and $b -lt 0x7F) {
                Write-Host ("  <- ascii '" + ([char]$b) + "'") -ForegroundColor DarkGray
            } else {
                Write-Host ("  <- 0x{0:X2}" -f $b) -ForegroundColor DarkGray
            }
        }
    }
    Write-Host ("  !! timeout waiting for " + $label) -ForegroundColor Red
    return $false
}

# --- main ---
if (-not (Test-Path $BinPath)) { throw "bin not found: $BinPath" }
$fileBytes = [System.IO.File]::ReadAllBytes($BinPath)
$fileSize  = $fileBytes.Length
$fileName  = [System.IO.Path]::GetFileName($BinPath)
Write-Host ("File: " + $fileName + "  size=" + $fileSize + " B") -ForegroundColor Cyan

$port = New-Object System.IO.Ports.SerialPort $ComPort, $Baud, 'None', 8, 'One'
$port.ReadTimeout  = 500
$port.WriteTimeout = 2000
$port.Open()
try {
    # 清空残留
    $port.DiscardInBuffer(); $port.DiscardOutBuffer()

    Write-Host "Step 1: wait for 'C' (CRC mode request)" -ForegroundColor Yellow
    # send 0x55 wake-up, give BL ~1.5s to reply 'C', re-spam if needed
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    $gotC = $false
    while ((Get-Date) -lt $deadline -and -not $gotC) {
        $port.Write([byte[]]@([byte]0x55), 0, 1)
        $end = (Get-Date).AddMilliseconds(1500)
        while ((Get-Date) -lt $end -and -not $gotC) {
            try {
                $b = $port.ReadByte()
                if ($b -eq $CCH) {
                    Write-Host ("  <- 'C' (0x43)") -ForegroundColor Green
                    $gotC = $true
                }
            } catch { }
        }
    }
    if (-not $gotC) { throw "no 'C' within $TimeoutSec sec (board not in BL?)" }

    Write-Host "Step 2: send block 0 (filename + filesize)" -ForegroundColor Yellow
    $pl0 = New-Object byte[] 128
    $ascii = [System.Text.Encoding]::ASCII
    $nameBytes = $ascii.GetBytes($fileName)
    [Array]::Copy($nameBytes, 0, $pl0, 0, [Math]::Min($nameBytes.Length, 100))
    $offset = $nameBytes.Length + 1
    $sizeStr = $fileSize.ToString()
    $sizeBytes = $ascii.GetBytes($sizeStr)
    [Array]::Copy($sizeBytes, 0, $pl0, $offset, $sizeBytes.Length)
    Write-Pkt $port ([byte]$SOH) ([byte]0x00) $pl0
    if (-not (Wait-For $port $ACK 5000 "ACK#0")) { throw "no ACK after block 0" }
    if (-not (Wait-For $port $CCH 5000 "'C' (data phase)")) { throw "no 'C' before data phase" }

    $nblocks = [Math]::Ceiling($fileSize / 1024.0)
    Write-Host ("Step 3: send " + $nblocks + " STX/1024B data blocks") -ForegroundColor Yellow
    for ($i = 0; $i -lt $nblocks; $i++) {
        $pl = New-Object byte[] 1024
        for ($k = 0; $k -lt 1024; $k++) { $pl[$k] = 0x1A }   # pad with CPMEOF
        $copyLen = [Math]::Min(1024, $fileSize - $i*1024)
        [Array]::Copy($fileBytes, $i*1024, $pl, 0, $copyLen)
        $seq = [byte](($i + 1) -band 0xFF)
        Write-Host ("  -> block " + ($i+1) + "  seq=0x{0:X2}  copy=" + $copyLen + " B" -f $seq)
        Write-Pkt $port ([byte]$STX) $seq $pl
        if (-not (Wait-For $port $ACK 8000 ("ACK#" + ($i+1)))) { throw "no ACK after block $($i+1)" }
    }

    Write-Host "Step 4: send EOT, expect NAK, send EOT again, expect ACK + 'C'" -ForegroundColor Yellow
    $port.Write([byte[]]@($EOT), 0, 1)
    if (-not (Wait-For $port $NAK 3000 "NAK after EOT#1")) { Write-Host "  (some impls skip NAK)" }
    $port.Write([byte[]]@($EOT), 0, 1)
    if (-not (Wait-For $port $ACK 3000 "ACK after EOT#2")) { throw "no ACK after EOT#2" }
    if (-not (Wait-For $port $CCH 3000 "'C' (closing)")) { throw "no closing 'C'" }

    Write-Host "Step 5: send closing block (filename empty)" -ForegroundColor Yellow
    $end = New-Object byte[] 128
    Write-Pkt $port ([byte]$SOH) ([byte]0x00) $end
    if (-not (Wait-For $port $ACK 3000 "final ACK")) { throw "no final ACK" }

    Write-Host "YMODEM transfer OK." -ForegroundColor Green
} finally {
    $port.Close()
}
