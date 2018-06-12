#Requires -RunAsAdministrator

$llvm_installer = 'LLVM-6.0.0-win64.exe'
$llvm_url = "http://releases.llvm.org/6.0.0/${llvm_installer}" # Note the lack of https
$llvm_md5 = 'f9523838bf214db2959c1d88d3b79d4d'

Write-Host "Downloading LLVM... ($llvm_url)"
Invoke-WebRequest $llvm_url -OutFile $llvm_installer

# Verify hash
$download_md5 = Get-FileHash $llvm_installer -Algorithm MD5
if ($download_md5.Hash -ne $llvm_md5)
{
    Write-Host "Error: MD5 mismatch"
    Exit 1
}

# Execute installer
Write-Host "Installing LLVM..."
Invoke-Expression "& .\$llvm_installer | Out-Null"