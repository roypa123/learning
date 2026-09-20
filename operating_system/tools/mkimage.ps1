<#
=============================================================================
  tools/mkimage.ps1  --  assemble a raw disk image out of flat binaries
=============================================================================

  A disk image is not a container format. There is no header, no index, no
  metadata: it is a byte-for-byte picture of what the sectors of a disk
  contain, and the only structure it has is the structure the boot process
  agrees on in advance. Sector 0 is what the BIOS reads. Sector 5 is where our
  stage 2 was told the kernel lives. That agreement lives in three places --
  boot.asm, stage2.asm, and this script -- and if they ever disagree, the
  machine reboots with no message.

  Usage:
      mkimage.ps1 -Out spark.img -Size 1474560 -Parts boot.bin:0,stage2.bin:1,kernel.bin:5

  -Size is in bytes; 1474560 is a 1.44 MiB floppy (80 cyl * 2 heads * 18 sect
  * 512 bytes). QEMU infers the geometry from the size, so getting this exact
  is what makes the CHS arithmetic in boot.asm come out right.

  Each -Parts entry is "file:LBA" -- the file is written starting at sector
  LBA. Overlaps are an error, not a warning, because an overlap means one of
  your components has grown past the space you reserved for it and the symptom
  is a machine that boots to a blank screen.

  Explained in: docs/08-stage2-loader.md
=============================================================================
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]   $Out,
    [Parameter(Mandatory = $true)] [int]      $Size,
    [Parameter(Mandatory = $true)] [string[]] $Parts,
    [int] $SectorSize = 512
)

$ErrorActionPreference = 'Stop'

if ($Size % $SectorSize -ne 0) {
    throw "Image size $Size is not a multiple of the sector size $SectorSize."
}

# A fresh image is all zeros. Real disks are full of whatever was there before,
# which is a good way to have a bug that only reproduces on one machine.
$image = New-Object byte[] $Size

# Track which sectors are occupied so we can refuse to silently overlap.
$owner = New-Object string[] ($Size / $SectorSize)

foreach ($part in $Parts) {
    $split = $part -split ':'
    if ($split.Count -ne 2) { throw "Bad -Parts entry '$part'; expected file:LBA" }

    $file = $split[0]
    $lba  = [int]$split[1]

    if (-not (Test-Path -LiteralPath $file)) { throw "Missing input file: $file" }

    $bytes  = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $file))
    $offset = $lba * $SectorSize

    # Round the length up to a whole number of sectors: a disk cannot hold half
    # a sector, and the loader always reads whole ones.
    $sectors = [math]::Ceiling($bytes.Length / [double]$SectorSize)

    if ($offset + ($sectors * $SectorSize) -gt $Size) {
        throw ("'$file' is {0} bytes at LBA {1}; that runs past the end of a {2}-byte image." -f $bytes.Length, $lba, $Size)
    }

    for ($s = 0; $s -lt $sectors; $s++) {
        $idx = $lba + $s
        if ($owner[$idx]) {
            throw ("Sector {0}: '$file' overlaps '{1}'. One of them has outgrown its reserved space." -f $idx, $owner[$idx])
        }
        $owner[$idx] = $file
    }

    [System.Array]::Copy($bytes, 0, $image, $offset, $bytes.Length)

    "{0,-24} LBA {1,-5} {2,7} bytes  ({3} sectors)" -f $file, $lba, $bytes.Length, $sectors | Write-Host
}

# The boot sector must end in 0x55 0xAA or the BIOS will not execute it -- it
# will print "no bootable device" and you will spend twenty minutes assuming
# your code crashed. Check it here, where the message can be useful.
if ($image[510] -ne 0x55 -or $image[511] -ne 0xAA) {
    throw ("Sector 0 does not end with the 0x55 0xAA boot signature (found {0:X2} {1:X2}). Is boot.bin at LBA 0?" -f $image[510], $image[511])
}

$outDir = Split-Path -Parent $Out
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

[System.IO.File]::WriteAllBytes(
    [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Out)), $image)

Write-Host ("-> {0} ({1} bytes, {2} sectors)" -f $Out, $Size, ($Size / $SectorSize))
