param($b, $a, [switch]$m, $p)
write-host $b.length, $a.length, $m, $p
$result = $true
if(($b.length -gt 0) -and ($a.length -gt 0))
	{
	esptool -p $p write_flash $a $b
	$result = $?
	}
if($result -and $m)
	{
	idf.py monitor -p $p
	}
else
	{
	write-host "esptool failed"
	}

