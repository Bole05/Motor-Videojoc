# Prepara la carpeta de la entrega y el zip:
#   Motor-Videojoc_v0.1/
#     Motor/          Motor.exe + DLLs + assets (sin código)
#     README.md
#     LICENCE.md
#     Licences/       licencias de las librerías de terceros
#
# Uso (desde la carpeta del proyecto):
#   powershell -ExecutionPolicy Bypass -File package_release.ps1
# Compila primero en Release con:
#   cmake --preset default
#   cmake --build build --config Release

$ErrorActionPreference = "Stop"
$version = "v0.1"
$name    = "Motor-Videojoc_$version"
$root    = $PSScriptRoot
$bin     = Join-Path $root "build\Release"
$out     = Join-Path $root $name

if (-not (Test-Path (Join-Path $bin "Motor.exe"))) {
    Write-Error "No se encuentra build\Release\Motor.exe. Compila antes en Release: cmake --build build --config Release"
}

if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $out "Motor") | Out-Null

# Ejecutable, DLLs y assets (sin .pdb, .lib, .exp ni ficheros intermedios)
Copy-Item (Join-Path $bin "Motor.exe") (Join-Path $out "Motor")
Get-ChildItem $bin -Filter *.dll | Copy-Item -Destination (Join-Path $out "Motor")
Copy-Item (Join-Path $bin "assets") (Join-Path $out "Motor\assets") -Recurse

# Documentación y licencias
Copy-Item (Join-Path $root "README.md")  $out
Copy-Item (Join-Path $root "LICENCE.md") $out
Copy-Item (Join-Path $root "Licences")   (Join-Path $out "Licences") -Recurse

# Zip
$zip = Join-Path $root "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $out -DestinationPath $zip
$mb = [math]::Round((Get-Item $zip).Length / 1MB, 2)
Write-Host "Creado $zip ($mb MB)"
if ($mb -gt 30) { Write-Warning "El zip pasa de 30 MB: quita modelos o texturas grandes de assets." }
