<?php
	include "functions.php";
	$image_width = 780;
	$image_height = 252;
	$player = "Refresh";
	$rank = "0";
	$discordcard = "-1";
	
	if (isset($_GET['player']))
		$player = $_GET['player'];
	if (isset($_GET['dicid']))
		$discordcard = $_GET['dicid'];
	if (isset($_GET['rank']))
		$rank = $_GET['rank'];
	
	// random string
	$str = "{I HAVE|I LOVE|SANTA BARBARA|BIG|SMALL|MEDIUM} {TEE|BIRD|GRANDMOTHER|FANTASY|TIGER ARR..}";
	while (strpos($str, "{") !== false)
	{
		$pos1 = strpos($str, "}");
		$buf_mas = substr($str, 0, $pos1);
		$pos2 = StrCharPosBack ($buf_mas, "{");
		$buf_mas = substr($buf_mas, $pos2+1);
		$exploded_buf_mas = explode ("|", $buf_mas);
		$rnd = rand(0, count($exploded_buf_mas)-1);
		$str = substr_replace($str, $exploded_buf_mas[$rnd], $pos2, $pos1+1-$pos2);
	}

	// var font
	$font_normal = "fonts/normal.ttf";
	$font_bold = "fonts/bold.ttf";
	
	// new image
	$img = imagecreatefrompng("images/$discordcard.png"); 
	$white_color  = imagecolorallocatealpha($img, 255, 255, 255, 0);
	$random_color  = imagecolorallocatealpha($img, rand(200, 255), rand(200, 255), rand(200, 255), 0);
	$golden_color = imagecolorallocatealpha($img, 255, 180, 30, 0);
	$bordour_color = imagecolorallocatealpha($img, 80, 80, 80, 90);

	// desc
	$font_size_desc = 12;
	imagettftext($img, $font_size_desc, 0, 20, 40, $random_color, $font_normal, "DISCORD MMO TEE CARD");

	// player
	$font_size_player = 30; 
	if (strlen($player) >= 12)
		$font_size_player = 28;
	
	$positiosY = ($image_height / 2);
	imagettftext($img, $font_size_player, 0, 90, $positiosY, $random_color, $font_normal, "$player");

	// random text
	$font_size_fun_desc = 16;
	$positiosY = ($image_height / 2) + ($font_size_player) + 10;
	imagettftext($img, $font_size_fun_desc, 0, 90, $positiosY, $white_color, $font_bold, $str);
	
	// rank
	$font_size_rank = 40;
	$rank_centrelized_right = strlen($rank) * ($font_size_rank / 2 + 10);
	$rank_centrelized_x = ($image_width / 2 + 100) - $rank_centrelized_right;
	imagettftext($img, $font_size_rank, 0, $rank_centrelized_x, ($image_height / 2 + 25), $golden_color, $font_normal, "RANK #$rank");

	header("Content-type: image/png");
	imagepng($img);
	imagedestroy($img);
?>