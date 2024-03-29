#!/bin/bash

echo "Extract Silly Knight resource data"
set -u
# path location of this script
scriptpath=${0%/*}
pushd "$scriptpath" 1>/dev/null 2>&1
scriptpath="`pwd`"
popd 1>/dev/null 2>&1



sourcePath="${1:-"$scriptpath/DATA"}"
targetPath="${2:-"$scriptpath"}"

if [ "$#" == "0" ]; then
  echo "It is expected to have, by default, the Silly Knight DATA folder at $sourcePath"
  echo "You may change the DATA folder location by providing the path in parameter"
  echo "   example: "
  echo "      $0 [path/to/sillyknight/DATA [path/to/generated_bs3_assembly_source_file]]"
  echo "      by default the second parameter is $targetPath"
  echo "---"
fi

function bs3_compile
{
  "$scriptpath/../../bs3asm" -e /dev/null -r /dev/null "$1"
}

function extract_MAP
{
  echo "Map extract from $1 to $2"
  echo ' include "bs3core.inc"' > "${targetPath}/$2"
  echo '; Begin of Silly Knight maps : '$1  >> "${targetPath}/$2"
  echo ' org $E000' >> "${targetPath}/$2"
  i=0
  hexdump -v -e '" db "' -e '20/1 "%02d,"' -e '"\n"' "${sourcePath}/$1" |  sed -e 's/,$//g' | while read line; do
    echo " $line" >> "${targetPath}/$2"
    i=$((i+1))
    if [ "$i" == "12" ]; then
      echo "" >> "${targetPath}/$2"
      i=0
    fi
  done 
  bs3_compile "${targetPath}/$2"
}

function extract_fonts
{
  if [ "$3" != "width" ]; then
    echo "Fonts extract from $1 to $2 for $3"
    echo ' include "bs3core.inc"' > "${targetPath}/$2"
    echo "; Begin of Silly Knight $1 " >> "${targetPath}/$2"
    echo "; each line, 8 times 8 bits : 8x8 1bpp" >> "${targetPath}/$2"
  else
    echo "Font glyph width from $1 to $2"
  fi
  echo ' org $E000' >> "${targetPath}/$2"
  i=0
  c=0
  BIT0='0'
  BIT1='F'
  if [ "$3" == "mask" ]; then
    BIT0='F'
    BIT1='0'
  fi
  if [ "$3" != "width" ]; then 
    hexdump -v -e '1/1 "%02X"' -e '"\n"' "${sourcePath}/$1" | while read -r value; do
      printf "%8b\n" $(bc <<< "ibase=16; obase=2; $value") | sed 's^ ^0^g'
      echo ""
    done | while read binvalue; do
    if [ "$binvalue" != "" ]; then
      if [ "$i" == "0" ]; then
        echo "; font glyph #$c"
      fi
      i=$(( i + 1 )) 
      if [ "$i" == "8" ]; then i=0;c=$(( c + 1 )); fi
      if [ "$c" == "128" ]; then break; fi
      echo -n " db "
      b7=${binvalue:0:1}; b6=${binvalue:1:1}; b5=${binvalue:2:1}; b4=${binvalue:3:1}
      b3=${binvalue:4:1}; b2=${binvalue:5:1}; b1=${binvalue:6:1}; b0=${binvalue:7:1}
      if [ "$b7" == "0" ]; then echo -n '$'"$BIT0"; fi
      if [ "$b7" == "1" ]; then echo -n '$'"$BIT1"; fi
      if [ "$b6" == "0" ]; then echo -n "$BIT0,"; fi
      if [ "$b6" == "1" ]; then echo -n "$BIT1,"; fi
      if [ "$b5" == "0" ]; then echo -n '$'"$BIT0"; fi
      if [ "$b5" == "1" ]; then echo -n '$'"$BIT1"; fi
      if [ "$b4" == "0" ]; then echo -n "$BIT0,"; fi
      if [ "$b4" == "1" ]; then echo -n "$BIT1,"; fi
      if [ "$b3" == "0" ]; then echo -n '$'"$BIT0"; fi
      if [ "$b3" == "1" ]; then echo -n '$'"$BIT1"; fi
      if [ "$b2" == "0" ]; then echo -n "$BIT0,"; fi
      if [ "$b2" == "1" ]; then echo -n "$BIT1,"; fi
      if [ "$b1" == "0" ]; then echo -n '$'"$BIT0"; fi
      if [ "$b1" == "1" ]; then echo -n '$'"$BIT1"; fi
      if [ "$b0" == "0" ]; then echo    "$BIT0";  fi
      if [ "$b0" == "1" ]; then echo    "$BIT1";  fi
    fi
    
    done >> "${targetPath}/$2"
  else
    # generate font width
    i=0
    c=0
    hexdump -v -e '1/1 "%02X"' -e '"\n"' "${sourcePath}/$1" | while read -r value; do
      i=$(( i + 1 ))
      if [ $i -gt 1024 ]; then
        echo "; width for glyph #$c"
        echo ' db $'$value
        c=$(( c + 1 )) 
      fi
    done  >> "${targetPath}/$2"
  fi  
#   hexdump -v -e '" db "' -e '8/1 "$%02X,"' -e '"\n"' "${sourcePath}/$1" |  sed -e 's/,$//g' >> "${targetPath}/$2" 
   
  bs3_compile "${targetPath}/$2"
}

function extract_screen
{
  echo "Screen extract from $1 to $2"
  echo ' include "bs3core.inc"' > "${targetPath}/$2"
  echo '; Begin of Silly Knight screen '$1 >> "${targetPath}/$2"
  echo ' org $E000' >> "${targetPath}/$2"
  hexdump -v -e '" db "' -e '16/1 "%u,"' -e '"\n"' "${sourcePath}/$1" | sed -e 's/,$//g' >> "${targetPath}/$2"
  bs3_compile "${targetPath}/$2"
}

function extract_sprite_or_data
{
  echo "$4 $3 from $1 to $2"
  echo "; $4 $3 of $1" > "${targetPath}/$2"
  echo ' org $E000' >> "${targetPath}/$2"
  k=0
  spr=0
  bb=0
  sprcnt=0;
  limit=96
  nbdata=0
  if [ "$3" == "mask" ]; then
    cutselect="-c3-4,7-8,11-12,15-16,19-20"
  else
    cutselect="-c1-2,5-6,9-10,13-14,17-18"
  fi
  if [ "$4" == "Tile" ]; then
    sot=2
    sotr=1
    limit=84
  else
    sot=2
    sotr=1
    limit=96
  fi
  hexdump -v -e '10/1 "%02X"' -e '"\n"' "${sourcePath}/$1" | cut $cutselect | while read line; do
    if [ $spr -lt $limit ]; then

      takeaway=$((spr%sot))
      length=${#line}
      if [ "$bb" == "0" ]; then
        if [ "$takeaway" == "$sotr" ]; then 
          echo "; $4 image #$sprcnt" >> "${targetPath}/$2"
          sprcnt=$((sprcnt+1))
        fi
        bb=1
      fi
      transformed_line=" db "
      for ((i=0; i<$length; i+=2)); do
          if ((i+1 < length)); then
              transformed_line+='$'${line:i+1:1}${line:i:1},
          else
              transformed_line+='$'${line:i:1},
          fi
      done
      if [ "$takeaway" == "$sotr" ]; then
        nbdata=$((nbdata+1))
        if [ "$nbdata" == "9" ]; then
          nbdata=0
        else
          echo " $transformed_line" |  cut -c1-20 >> "${targetPath}/$2"
        fi
      fi
      
      k=$((k+1))
      if [ "$k" == "9" ]; then
        k=0
        bb=0
        spr=$((spr+1))
        echo "" >> "${targetPath}/$2"
      fi
    fi
  done 
  bs3_compile "${targetPath}/$2"
}

extract_MAP "00.MAP" "sk_map0.s" 
extract_MAP "01.MAP" "sk_map1.s"
extract_MAP "02.MAP" "sk_map2.s"
extract_MAP "03.MAP" "sk_map3.s"
extract_MAP "04.MAP" "sk_map4.s"
extract_MAP "05.MAP" "sk_map5.s"
extract_MAP "06.MAP" "sk_map6.s"
extract_MAP "07.MAP" "sk_map7.s"
extract_MAP "08.MAP" "sk_map8.s"
extract_MAP "09.MAP" "sk_map9.s"
extract_MAP "0A.MAP" "sk_mapa.s"
extract_MAP "0B.MAP" "sk_mapb.s"
extract_MAP "0C.MAP" "sk_mapc.s"
extract_MAP "0D.MAP" "sk_mapd.s"
extract_MAP "0E.MAP" "sk_mape.s"

extract_screen "DEATH.DAT" "sk_scr_death.s"
extract_screen "END.DAT" "sk_scr_end.s"
extract_screen "HV.DAT" "sk_scr_hv.s"
extract_screen "INTRO1.DAT" "sk_scr_intro1.s"
extract_screen "INTRO2.DAT" "sk_scr_intro2.s"
extract_screen "LOGO.DAT" "sk_scr_logo.s"
extract_screen "MENU.DAT" "sk_scr_menu.s"

extract_sprite_or_data "SPRITES.DAT" "sk_spr_data.s" "data" "Sprite"
extract_sprite_or_data "SPRITES.DAT" "sk_spr_mask.s" "mask" "Sprite"

extract_sprite_or_data "TILES.DAT" "sk_tile_data.s" "data" "Tile"
extract_sprite_or_data "TILES.DAT" "sk_tile_mask.s" "mask" "Tile"

extract_fonts "FONTS.DAT" "sk_fonts_data.s" data
extract_fonts "FONTS.DAT" "sk_fonts_mask.s" mask
extract_fonts "FONTS.DAT" "sk_fonts_width.s" width
