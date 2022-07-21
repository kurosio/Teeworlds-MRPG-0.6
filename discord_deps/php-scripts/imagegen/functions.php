<?php
	function StrCharPosBack($haystack, $needle) 
	{
		for ($i = strlen($haystack) ; $i>=0 ; $i--)
		{
			if (substr($haystack, $i, 1) === $needle)
				return $i;
		}
		return false;
	}
	function clamp($current, $min, $max) 
	{
		return max($min, min($max, $current));
	}
?>