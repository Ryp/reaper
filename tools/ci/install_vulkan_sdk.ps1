#Requires -RunAsAdministrator

$vulkan_sdk_version = "1.2.131.1"
$vulkan_sdk_installer = "VulkanSDK-${vulkan_sdk_version}-Installer.exe"
$vulkan_sdk_url = "https://www.dropbox.com/s/v8t0k2yce4h5ycp/${vulkan_sdk_installer}?dl=1"
$vulkan_sdk_md5 = 'c46b0a52552075841c171c3c990cdc1a'

Write-Host "Downloading Vulkan SDK... ($vulkan_sdk_url)"
Invoke-WebRequest $vulkan_sdk_url -OutFile $vulkan_sdk_installer

# Verify hash
$download_md5 = Get-FileHash $vulkan_sdk_installer -Algorithm MD5
if ($download_md5.Hash -ne $vulkan_sdk_md5)
{
   Write-Host "Error: MD5 mismatch"
   Exit 1
}

# Execute installer in silent mode and wait for completion
Write-Host "Installing Vulkan SDK..."
Invoke-Expression "& .\$vulkan_sdk_installer /S | Out-Null"

# Set VK_SDK_PATH env var for current shell
$env:VK_SDK_PATH = "c:\VulkanSDK\$vulkan_sdk_version"
