param(
    [Parameter(Mandatory)][ValidatePattern('^COM[1-9][0-9]*$')][string] $HubPort,
    [Parameter(Mandatory)][ValidateCount(2,2)][ValidatePattern('^COM[1-9][0-9]*$')][string[]] $NodePorts,
    [ValidateRange(1,1000)][int] $Rounds = 250,
    [ValidateRange(48,86400)][int] $CycleSeconds = 50,
    [ValidateRange(1,244)][int] $FirstTrialId = 1,
    [string] $OutputDirectory = (Join-Path (Get-Location) ("loom-utc-" + (Get-Date -Format 'yyyyMMdd-HHmmss')))
)

$ErrorActionPreference = 'Stop'
$ports = @($HubPort) + $NodePorts
if (@($ports | Sort-Object -Unique).Count -ne 3) { throw 'Provide three distinct COM ports.' }
$null = New-Item -ItemType Directory -Path $OutputDirectory
$resultsPath = Join-Path $OutputDirectory 'results.csv'
$devices = @()
$active = $false

function Read-Ports {
    foreach ($device in $devices) {
        $chunk = $device.Serial.ReadExisting()
        if ($chunk) {
            $device.Reply += $chunk
            [IO.File]::AppendAllText($device.Log, $chunk)
        }
    }
}

function Send-Line($Device, [string] $Command) {
    [IO.File]::AppendAllText($Device.Log, "> $Command" + [Environment]::NewLine)
    $Device.Serial.WriteLine($Command)
}

function Wait-ForReplies([string] $Pattern, [datetime] $Deadline, [string] $Phase) {
    $answers = @{}
    do {
        Read-Ports
        foreach ($device in $devices) {
            $errorLine = [regex]::Match($device.Reply, '(?m)^ERR [^\r\n]+')
            if ($errorLine.Success) { throw "$($device.Port) $($errorLine.Value)" }
            if (!$answers.ContainsKey($device.Port)) {
                $match = [regex]::Match($device.Reply, $Pattern)
                if ($match.Success) {
                    $answers[$device.Port] = [pscustomobject]@{
                        Text = $match.Value
                        ReceivedUtc = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
                    }
                }
            }
        }
        if ($answers.Count -eq $devices.Count) { return $answers }
        Start-Sleep -Milliseconds 150
    } while ((Get-Date) -lt $Deadline)
    $missing = ($devices | Where-Object { !$answers.ContainsKey($_.Port) }).Port -join ', '
    throw "$Phase timed out on $missing. See the serial logs."
}

function Parse-Metrics([string] $Line) {
    $metrics = @{}
    foreach ($pair in [regex]::Matches($Line, '([A-Za-z_]+)=([^\s]+)')) {
        $metrics[$pair.Groups[1].Value] = $pair.Groups[2].Value
    }
    return $metrics
}

try {
    foreach ($port in $ports) {
        $serial = [IO.Ports.SerialPort]::new($port, 115200)
        $serial.DtrEnable = $true
        $serial.WriteTimeout = 2000
        $device = [pscustomobject]@{ Port = $port; Serial = $serial; Reply = ''; Log = (Join-Path $OutputDirectory "$port.log") }
        $devices += $device
        $serial.Open()
    }
    Start-Sleep -Milliseconds 400
    foreach ($device in $devices) {
        Send-Line $device 'role'
        Send-Line $device 'status'
    }
    $identityPattern = '(?m)^(?:hub=1 offer=|node=[12] )[^\r\n]*\bradio=ready\b[^\r\n]*\r?\n'
    $null = Wait-ForReplies $identityPattern ((Get-Date).AddSeconds(8)) 'Device identification'
    if ($devices[0].Reply -notmatch '(?m)^role=hub\r?$' -or
        $devices[0].Reply -notmatch '(?m)^hub=1 offer=[^\r\n]*radio=ready idle lte_build=no[^\r\n]*enrollment=off') {
        throw 'The hub must be idle with LTE disabled and fixed IDs.'
    }
    $ids = @()
    foreach ($device in $devices[1..2]) {
        $identity = [regex]::Match($device.Reply, '(?m)^node=([12]) [^\r\n]*\bpreferred_hub=1\b[^\r\n]*\bid_mode=fixed\b')
        if (!$identity.Success -or $device.Reply -notmatch '(?m)^role=node\r?$') {
            throw "$($device.Port) must be a fixed-ID node with hub 1 as its preferred hub."
        }
        $ids += $identity.Groups[1].Value
    }
    if (@($ids | Sort-Object -Unique).Count -ne 2) { throw 'Expected node IDs 1 and 2.' }

    $trial = $FirstTrialId
    foreach ($power in 5,14,23) {
        foreach ($ack in 100,500) {
            foreach ($mode in 'ram','sd') {
                Read-Ports
                $epoch = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() + 40
                $command = "utc $mode $epoch $Rounds $ack $power $CycleSeconds $trial"
                $active = $true
                Write-Host "Trial $trial | $mode | $power dBm | $ack ms | $($Rounds * 2) expected readings"
                foreach ($device in $devices) {
                    $device.Reply = ''
                    Send-Line $device $command
                }
                $armPattern = "(?m)^UTC start=$epoch now=\d+ cycle_s=$CycleSeconds slot_s=\d+ nodes=2 retries=10\r?\n"
                $armed = Wait-ForReplies $armPattern ((Get-Date).AddSeconds(15)) 'UTC arm'
                $offsets = @()
                foreach ($response in $armed.Values) {
                    $clock = Parse-Metrics $response.Text
                    $offsets += [long]$clock.now - $response.ReceivedUtc
                }
                $clockRange = $offsets | Measure-Object -Minimum -Maximum
                if ($clockRange.Maximum - $clockRange.Minimum -gt 1 -or
                    $clockRange.Minimum -lt -2 -or $clockRange.Maximum -gt 2) {
                    throw 'Synchronize the three board clocks to UTC before running the matrix.'
                }
                $deadline = [DateTimeOffset]::FromUnixTimeSeconds($epoch).LocalDateTime.AddSeconds(
                    ($Rounds + 2) * $CycleSeconds + 60)
                $finished = Wait-ForReplies "(?m)^UTC END start=$epoch [^\r\n]*\r?\n" $deadline 'UTC trial'
                $summary = Parse-Metrics $finished[$HubPort].Text
                $nodeMetrics = @($devices[1..2] | ForEach-Object { Parse-Metrics $finished[$_.Port].Text })
                foreach ($row in @($summary) + $nodeMetrics) {
                    if ([int]$row.cycles -ne $Rounds -or [int]$row.drain -ne 2 -or
                        [int]$row.fault -ne 0 -or $row.mode -ne $mode -or
                        [int]$row.trial -ne $trial -or [int]$row.ack_ms -ne $ack -or [int]$row.tx_dBm -ne $power) {
                        throw "Trial $trial ended with incomplete or mismatched settings. See the serial logs."
                    }
                }
                foreach ($row in $nodeMetrics) {
                    foreach ($key in 'link_datagrams','link_datagrams_retried','link_retries','pending') {
                        if (!$row.ContainsKey($key)) { throw "Trial $trial is missing node metric $key." }
                    }
                }
                $active = $false
                $expected = $Rounds * 2
                $received = [int]$summary.records
                [pscustomobject]@{
                    Trial = $trial
                    Sd = ($mode -eq 'sd')
                    PowerDbm = $power
                    AckMs = $ack
                    Retries = 10
                    Expected = $expected
                    OnTime = $received - [int]$summary.backfilled
                    Backfilled = [int]$summary.backfilled
                    Delivered = $received
                    Missing = $expected - $received
                    Duplicates = [int]$summary.duplicates
                    DatagramsAttempted = ($nodeMetrics | ForEach-Object { [int]$_.link_datagrams } | Measure-Object -Sum).Sum
                    DatagramsRetried = ($nodeMetrics | ForEach-Object { [int]$_.link_datagrams_retried } | Measure-Object -Sum).Sum
                    DatagramRetryAttempts = ($nodeMetrics | ForEach-Object { [int]$_.link_retries } | Measure-Object -Sum).Sum
                    PendingOnSd = ($nodeMetrics | ForEach-Object { [int]$_.pending } | Measure-Object -Sum).Sum
                } | Export-Csv -LiteralPath $resultsPath -NoTypeInformation -Append
                Write-Host "Trial $trial finished with $received/$expected readings."
                ++$trial
            }
        }
    }
    Write-Host "Results saved to $resultsPath"
} finally {
    foreach ($device in $devices) {
        if ($active -and $device.Serial.IsOpen) {
            try { Send-Line $device 'utc stop' } catch { Write-Warning "$($device.Port) could not be stopped over USB." }
        }
        $device.Serial.Dispose()
    }
}
