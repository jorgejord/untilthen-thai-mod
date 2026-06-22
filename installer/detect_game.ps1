# Auto-detect the "Until Then" game folder across all Steam libraries. ASCII only (PS 5.1 safe).
$ErrorActionPreference = 'SilentlyContinue'
$cands = New-Object System.Collections.Generic.List[string]

# 1) Steam install path from registry
$sp = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -Name SteamPath).SteamPath
if (-not $sp) { $sp = (Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam' -Name InstallPath).InstallPath }
if ($sp) { $cands.Add($sp) }

# 2) every library listed in libraryfolders.vdf
if ($sp) {
  $vdf = Join-Path $sp 'steamapps\libraryfolders.vdf'
  if (Test-Path $vdf) {
    foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s*"([^"]+)"')) {
      $cands.Add($m.Groups[1].Value.Replace('\\','\'))
    }
  }
}

# 3) common fallback locations
'C:\Program Files (x86)\Steam','C:\Program Files\Steam','D:\Steam','D:\SteamLibrary',
'E:\Steam','E:\SteamLibrary','F:\Steam','F:\SteamLibrary' | ForEach-Object { $cands.Add($_) }

foreach ($c in $cands) {
  if (-not $c) { continue }
  $g = Join-Path $c 'steamapps\common\Until Then'
  if ((Test-Path (Join-Path $g 'UntilThen.pck')) -or (Test-Path (Join-Path $g 'UntilThen.pck.bak'))) {
    Write-Output $g
    exit 0
  }
}
exit 1
