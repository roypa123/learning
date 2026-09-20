<#
=============================================================================
  tools/mkinitrd.ps1  --  build a ustar archive with no tar command
=============================================================================

  Windows has `tar.exe` these days, but it produces archives with PAX
  extension records that our eighty-line reader would have to learn to skip.
  Writing the archive ourselves keeps the format to exactly what fs/initrd.c
  parses, and -- more usefully -- means you can read this script alongside the
  reader and see both halves of the format at once.

  A ustar entry is:

      512 bytes of header, mostly ASCII, mostly octal
      the file's contents
      zero padding up to the next 512-byte boundary

  and the archive ends with two zero blocks.

  Usage:
      mkinitrd.ps1 -Out initrd.tar -Files bin\sh,bin\ls,etc\motd

  Explained in: docs/40-initrd.md
=============================================================================
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]   $Out,
    [Parameter(Mandatory = $true)] [string[]] $Files
)

$ErrorActionPreference = 'Stop'

$stream = [System.IO.MemoryStream]::new()

function Write-Field {
    param([byte[]]$Header, [int]$Offset, [string]$Text, [int]$Width)

    $bytes = [System.Text.Encoding]::ASCII.GetBytes($Text)
    $n = [Math]::Min($bytes.Length, $Width)
    [System.Array]::Copy($bytes, 0, $Header, $Offset, $n)
}

foreach ($file in $Files) {
    if (-not (Test-Path -LiteralPath $file)) { throw "mkinitrd: missing $file" }

    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $file))
    $name  = (Split-Path -Leaf $file)

    if ($name.Length -gt 99) { throw "mkinitrd: name too long for ustar: $name" }

    $header = New-Object byte[] 512

    Write-Field $header   0 $name                              100  # name
    Write-Field $header 100 "0000644`0"                          8  # mode, octal
    Write-Field $header 108 "0000000`0"                          8  # uid
    Write-Field $header 116 "0000000`0"                          8  # gid

    # Size: 11 octal digits and a NUL. This is the field that makes people
    # assume tar is binary and then wonder why their 9-byte file reads as 11.
    Write-Field $header 124 ("{0}`0" -f [Convert]::ToString($bytes.Length, 8).PadLeft(11, '0')) 12
    Write-Field $header 136 "00000000000`0"                     12  # mtime
    Write-Field $header 156 "0"                                  1  # typeflag: regular file
    Write-Field $header 257 "ustar`0"                            6  # magic
    Write-Field $header 263 "00"                                 2  # version

    # The checksum is computed with the checksum field itself treated as eight
    # spaces, then written back into that field. A self-referential definition
    # that every tar implementation gets right and every first attempt gets
    # wrong -- and a wrong checksum is why GNU tar says "invalid header".
    for ($i = 148; $i -lt 156; $i++) { $header[$i] = 0x20 }

    $sum = 0
    foreach ($b in $header) { $sum += $b }

    Write-Field $header 148 ("{0}`0 " -f [Convert]::ToString($sum, 8).PadLeft(6, '0')) 8

    $stream.Write($header, 0, 512)
    $stream.Write($bytes, 0, $bytes.Length)

    # Pad the data out to a 512-byte boundary.
    $pad = (512 - ($bytes.Length % 512)) % 512
    if ($pad -gt 0) { $stream.Write((New-Object byte[] $pad), 0, $pad) }

    "{0,-20} {1,8} bytes" -f $name, $bytes.Length | Write-Host
}

# Two zero blocks mark the end of the archive.
$stream.Write((New-Object byte[] 1024), 0, 1024)

$full = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Out))
[System.IO.File]::WriteAllBytes($full, $stream.ToArray())

Write-Host ("-> {0} ({1} bytes)" -f $Out, $stream.Length)
