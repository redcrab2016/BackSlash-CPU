    include "bs3core.inc"
    include "bs3gfx.inc"

        ; ROM resources
    ; Screen shots
; ROM screen shot address    
rom_scr_addr        equ     $E000    

; Screen shot ROM bank number
rom_sk_scr_death    equ     0
rom_sk_scr_end      equ     1
rom_sk_scr_logo     equ     2
rom_sk_scr_hv       equ     3
rom_sk_scr_intro1   equ     4
rom_sk_scr_intro2   equ     5
rom_sk_scr_menu     equ     6

    ; Game maps
; map layer address
rom_map_layer0_addr    equ     $E000
rom_map_layer1_offset  equ     $00F0
rom_map_layer2_offset  equ     $01E0

; map navigation
; x  -> y, z, s ; means the map x express way to go to map y, z and s
; 0  -> 1, 5
; 1  -> 0, 2
; 2  -> 1, 3, 5
; 3  -> 2, 4, 5, 7
; 4  -> 3, 6, 8
; 5  -> 0, 2, 3
; 6  -> 4
; 7  -> 3, 10
; 8  -> 4, 9
; 9  -> 8
; 10 -> 7, 11
; 11 -> 10, 12
; 12 -> 11, 13
; 13 -> 12
; ?? -> 14 (14 is an empty map)
; 14 -> ??
; map layout 
;  3 x (20 x 12) blocks
; 1st (20x12) blocks is tile reference
; 1st       2nd     3rd are   map metadata
; ?          0       ?  : Air ... can move into
; ?          1       ?  : player ground/wall 
; ?          2       ?  : Ledder, stairs, platform(can go down)
; when go to m2 from m1 then x,y corrdinates at m2
;  is where there is a "go to " m1
;  but if coords is at left border then move 1 block to the right
;      if coords is at right border then move 1 block to the left 
;  7         3       m  : door to go map 'm' (?m(x,y))
;  0         3       m  : border to go map 'm' (?m(x,y))

;  ?         4       0  : item, health (max health +1)
;  ?         4       1  : item, silver key
;  ?         4       2  : item, green key
;  ?         4       3  : item, red key
;  ?         4       4  : item, blue key
;  ?         4       5  : item, purple key
;  ?         4       6  : item, gold key
;  ?         4       7  : item, health plus (max health +1)
;  ?         4       8  : item, boots (double jump)
;  ?         4       9  : item, diamond (metamorphose bat)
;  ?         5       0  : enemy, bat
;  ?         5       1  : enemy, skeleton
;  ?         5       2  : enemy, red knight
;  ?         6       x  : health -= x
;  ?         7       0  : metamorphose hero to bat (if item diamond)
;  ?         7       1  : metamorphose bat to hero
;  ?         7      15  : game win, if all enemies are dead on map
;  28        8       ?  : healing + savepoint
;  ?         9       0  : emitter, fireball right
;  ?         9       1  : emitter, fireball left
;  ?         9       2  : emitter, fireskull right
;  ?         9       3  : emitter, fireskull left
;  ?         10      1  : lock, silver 
;  ?         10      2  : lock, green
;  ?         10      3  : lock, red
;  ?         10      4  : lock, blue
;  ?         10      5  : lock, purple
;  ?         10      6  : lock, gold


; Map ROM bank number
rom_sk_map0         equ     7
rom_sk_map1         equ     8
rom_sk_map2         equ     9   
rom_sk_map3         equ     10
rom_sk_map4         equ     11
rom_sk_map5         equ     12
rom_sk_map6         equ     13
rom_sk_map7         equ     14
rom_sk_map8         equ     15
rom_sk_map9         equ     16
rom_sk_mapa         equ     17
rom_sk_mapb         equ     18
rom_sk_mapc         equ     19
rom_sk_mapd         equ     20
rom_sk_mape         equ     21

    ; Sprites data and mask
rom_sk_spr_data     equ     22
rom_sk_spr_mask     equ     23

    ; Tiles data and mask
rom_sk_tile_data    equ     24
rom_sk_tile_mask    equ     25
rom_sk_fonts        equ     26

    ; Embedded ROMs
    embed   "sk_scr_death.bs3",     rom_sk_scr_death,   0
    embed   "sk_scr_end.bs3",       rom_sk_scr_end,     0
    embed   "sk_scr_logo.bs3",      rom_sk_scr_logo,    0
    embed   "sk_scr_hv.bs3",        rom_sk_scr_hv,      0
    embed   "sk_scr_intro1.bs3",    rom_sk_scr_intro1,  0
    embed   "sk_scr_intro2.bs3",    rom_sk_scr_intro2,  0
    embed   "sk_scr_menu.bs3",      rom_sk_scr_menu,    0
    embed   "sk_map0.bs3",          rom_sk_map0,        0
    embed   "sk_map1.bs3",          rom_sk_map1,        0
    embed   "sk_map2.bs3",          rom_sk_map2,        0   
    embed   "sk_map3.bs3",          rom_sk_map3,        0
    embed   "sk_map4.bs3",          rom_sk_map4,        0
    embed   "sk_map5.bs3",          rom_sk_map5,        0
    embed   "sk_map6.bs3",          rom_sk_map6,        0
    embed   "sk_map7.bs3",          rom_sk_map7,        0
    embed   "sk_map8.bs3",          rom_sk_map8,        0
    embed   "sk_map9.bs3",          rom_sk_map9,        0
    embed   "sk_mapa.bs3",          rom_sk_mapa,        0
    embed   "sk_mapb.bs3",          rom_sk_mapb,        0
    embed   "sk_mapc.bs3",          rom_sk_mapc,        0
    embed   "sk_mapd.bs3",          rom_sk_mapd,        0
    embed   "sk_mape.bs3",          rom_sk_mape,        0
    embed   "sk_spr_data.bs3",      rom_sk_spr_data,    0
    embed   "sk_spr_mask.bs3",      rom_sk_spr_mask,    0
    embed   "sk_tile_data.bs3",     rom_sk_tile_data,   0
    embed   "sk_tile_mask.bs3",     rom_sk_tile_mask,   0
    embed   "sk_fonts.bs3",         rom_sk_fonts,       0


sk_tile_keycolor        equ     232
;tiles for sprites : defined by a word (16 bits)
; MSB : bank number
; LSB : index number
sk_spr_sword            equ    $0100 ; Sword
sk_spr_heart_red        equ    $0101 ; red heart
sk_spr_heart_empty      equ    $0102 ; empty heart
sk_spr_key_gray         equ    $0103 ; gray key
sk_spr_key_green        equ    $0104 ; green key
sk_spr_key_red          equ    $0105 ; red key
sk_spr_key_blue         equ    $0106 ; blue key
sk_spr_key_purple       equ    $0107 ; purple key
sk_spr_key_yellow       equ    $0108 ; yellow key
sk_spr_hero_right1      equ    $0109 ; hero face to right open legs
sk_spr_hero_right2      equ    $010A ; hero face to right close legs
sk_spr_hero_left1       equ    $010B ; hero face to left open legs
sk_spr_hero_left2       equ    $010C ; hero face to left close legs
sk_spr_heart_yellow     equ    $010D ; yellow heart
sk_spr_bottle_gray      equ    $010E ; gray bottle
sk_spr_horse_right1     equ    $010F ; horse face to right close legs
sk_spr_horse_right2     equ    $0110 ; horse face to right open legs
sk_spr_hero_right_ride  equ    $0111 ; hero face to right riding 
sk_spr_bat_sleep        equ    $0112 ; bat sleep
sk_spr_bat_fly1         equ    $0113 ; bat fly wings up
sk_spr_bat_fly2         equ    $0114 ; bat fly wings down
sk_spr_skelguard_right1 equ    $0115 ; sword skeleton right open legs
sk_spr_skelguard_right2 equ    $0116 ; sword skeleton right close legs
sk_spr_skelguard_left1  equ    $0117 ; sword skeleton left open legs
sk_spr_skelguard_left2  equ    $0118 ; sword skeleton left close legs
sk_spr_guard_right1     equ    $0119 ; sword guard right open legs
sk_spr_guard_right2     equ    $011A ; sword guard right close legs
sk_spr_guard_left1      equ    $011B ; sword guard left open legs
sk_spr_guard_left2      equ    $011C ; sword guard left close legs
sk_spr_picks_ground     equ    $011D ; surface small picks
sk_spr_skeleton_right1  equ    $011E ; Skeleton face to right open legs
sk_spr_skeleton_right2  equ    $011F ; Skeleton face to left close legs
sk_spr_skeleton_left1   equ    $0120 ; Skeleton face to left open legs
sk_spr_skeleton_left2   equ    $0121 ; Skeleton face to left close legs
sk_spr_fireskull_right  equ    $0122 ; fire skull face to right
sk_spr_fireskull_left   equ    $0123 ; fire skull face to left
sk_spr_herosword_right  equ    $0124 ; Hero sword attack to right
sk_spr_herosword_left   equ    $0125 ; Hero sword attack to left
sk_spr_fireball_right   equ    $0126 ; fireball to right
sk_spr_fireball_left    equ    $0127 ; fireball to left
sk_spr_whiteball        equ    $0128 ; white ball
sk_spr_circle_small     equ    $0129 ; small gray circle
sk_spr_circle_medium    equ    $012A ; medium gray circle
sk_spr_circle_large     equ    $012B ; large gray circle
sk_spr_fire_left        equ    $012C ; furnace fire danse left
sk_spr_fire_right       equ    $012D ; furnace fire danse right
sk_spr_boots            equ    $012E ; boots
sk_spr_book             equ    $012F ; spell book


; Entry point
            mbs3_bootat start
start:
            mbs3_gfx_reset
            ; load tiles and sprite into gfx device
            call            transromTileSpriteToGFX
            mov             b0, rom_sk_scr_hv
            call            showromscreenshot
            mbs3_gfx_refresh
            mbs3_wait_input
            mov             b0, rom_sk_scr_logo
            call            showromscreenshot
            mbs3_gfx_refresh
            mbs3_wait_input
            mov             b0, rom_sk_scr_menu
            call            showromscreenshot
            mbs3_gfx_refresh
            mbs3_wait_input
            mov             b0, rom_sk_scr_intro1
            call            showromscreenshot
            mbs3_gfx_refresh
            mbs3_wait_input
            mov             b0, rom_sk_scr_intro2
            call            showromscreenshot
            mbs3_gfx_refresh
            mbs3_wait_input

            ; show a sprite
            mbs3_gfx_setPB1 0                   ; sprite 0
            mbs3_gfx_setPW2 $3050               ; coords x=80,y=48
            mbs3_gfx_setPB3 1                   ; tile surface
            mbs3_gfx_setPW4 sk_spr_hero_left1   ; set index/bank
            mbs3_gfx_setPB5 sk_tile_keycolor    ; keycolor
            mbs3_gfx_setPB6 $A1                 ; enabled|z=1|keycolor
            mbs3_gfx_sprconf                    ; sprite config

            ; shows all maps
            mov             b1, 0
.cont            
            ; clear viewport
            mbs3_gfx_setPB1     16
            mbs3_gfx_vpclear
            ; show map "b1"
.cont2      mov             b0,        b1 ; map "b1"
            call            sk_tilemap ; sk_map_blit
            mbs3_gfx_refresh
            mbs3_wait_input
            inc             b1
            cmp             b1, 15
            jnz             .cont2 ; .cont

            ; hide tile map 0
            mbs3_gfx_setPB1 0
            mbs3_gfx_setPB2 0
            mbs3_gfx_tmconfig
            ; below is code to show the surface 1 (tile and spr banks)
            mbs3_gfx_vpgetconf
            mbs3_gfx_setPB1 1
            mbs3_gfx_vpconfig
            mbs3_gfx_refresh
            mbs3_wait_input

            mbs3_gfx_vpgetconf
            mbs3_gfx_setPB1 1
.blop
            mov             w1, 0
.again1     mbs3_gfx_setPW2     w1
            mbs3_gfx_vpconfig
            mbs3_gfx_refresh
            inc             b2
            cmp             b2, $60
            jnz             .again1

            mov             w1, $0060
.again2     mbs3_gfx_setPW2     w1
            mbs3_gfx_vpconfig
            mbs3_gfx_refresh
            dec             b2
            cmp             b2, $ff
            jnz             .again2

            in              b0
            jnz              .cont3
            jump            .blop

.cont3            
            ; mbs3_wait_input




.byebye            
            mbs3_gfx_end
            hlt

; show ROM screen shot in current view port
; use B0 as ROM bank number that
; contains the screen shot to show
; after calling this routine, a GFX refresh is needed
showromscreenshot:
            ; save env
            pusha
            ld      b1, [lbs3_bank_rom]
            sr      b1, [.oldrom]
            ; set rom bank with screenshot rombank
            sr      b0, [lbs3_bank_rom]
            ; get current view port surface,coords,size
            mbs3_gfx_vpgetconf
            ; set GFX reg PW4 = PW3
            ld      w1, [lbs3_gfx_reg3]
            mbs3_gfx_setPW4 w1
            ; start screen address
            mov     w3, rom_scr_addr
            ; start pixel transfer
            C       sk_scrblitrans
            ; restore env
            ld      b1, [.oldrom]
            sr      b1, [lbs3_bank_rom]
            popa
            ret
.oldrom      db      0    

sk_scrblitrans
                mbs3_gfx_blitrans
                cmp                 b0, ebs3_gfx_code_ok ; is it ok ?
                jnz                 .endblitrans ; not ok then quit
.transferloop                
                ld                  b0, [lbs3_gfx_status]
                tst                 b0, ebs3_gfx_status_waitdata
                jz                  .endblitrans ; end of transfer
                ld                  b0, [w3]
                mov                 b1, b0
                and                 b0, $0F 
                shr                 b1
                shr                 b1
                shr                 b1
                shr                 b1
                mbs3_gfx_setPB3     b0
                call                lbs3_gfx_busywait
                mbs3_gfx_setPB3     b1
                call                lbs3_gfx_busywait
                inc                 w3
                j                   .transferloop
.endblitrans
                ret

; draw a sk map
; parameter
;    b0 : map number
sk_map_blit
                ; save env
                pusha
                ld                  b1, [lbs3_bank_rom]
                sr                  b1, [.oldrom]
                ; select rom map
                add                 b0, rom_sk_map0
                sr                  b0, [lbs3_bank_rom]
                ; select gfx image bank
                mov                 b1, 0 ; tile gfx bank
                ; first tile index address
                mov                 w2, rom_map_layer0_addr
                ; first gfx coordinate
                mov                 w1, $0000
                ; get tile index
.blit           ld                  b0, [w2] ; get image index
                ; blit tile b0 at w1(b3b2) coordinate on surface 0
                call                blit8x8frombank ; blt tile b0 @ w1
                inc                 w2 ; next tile address
                add                 b2, 8
                cmp                 b2, 160
                jnz                 .blit
                eor                 b2, b2
                add                 b3, 8
                cmp                 b3, 96
                jnz                 .blit
                ; restore env
                ld                  b1, [.oldrom]
                sr                  b1, [lbs3_bank_rom]
                popa
                ret
.oldrom         db                  0

; tile map load from sk map
; parameter
;    b0 : sk map number
sk_tilemap
                ; save env
                pusha
                ld                  b1, [lbs3_bank_rom]
                sr                  b1, [.oldrom]
                ; select rom map
                add                 b0, rom_sk_map0
                sr                  b0, [lbs3_bank_rom]
                ; select gfx image bank
                mov                 b1, 0 ; tile gfx bank
                ; first tile index address in rom
                mov                 w2, rom_map_layer0_addr
                ; first tilemap coordinates
                mov                 w1, $0000
                ; reset tile maps
                mbs3_gfx_setPB1     0 ; choose tilemap 0
                mbs3_gfx_tmreset      ; reset chosen tilemap
                ; make visible tile map 0
                mbs3_gfx_setPB2     1 ; visible
                mbs3_gfx_tmconfig     ; set chosen tilemap config
                ; common register for all tiles in map
                mbs3_gfx_setPB3     1   ; surface where are tiles
                mbs3_gfx_setPB5     sk_tile_keycolor ; keycolor
                mbs3_gfx_setPB6     $81 ; visible | use keycolor
                ; get tile index
.blit           ld                  b0, [w2] ; get image index
                mov                 b1, 0    ; tile bank 0
                mbs3_gfx_setPW4     w0       ; set tile bank and index
                mbs3_gfx_setPW2     w1       ; tile map coordinates
                mbs3_gfx_tmcellconf          ; set tile into tilemap
                inc                 w2 ; next rom map tile address
                ; next tilemap coordinates
                inc                 b2
                cmp                 b2, 20
                jnz                 .blit
                eor                 b2, b2
                inc                 b3
                cmp                 b3, 12
                jnz                 .blit
                ; restore env
                ld                  b1, [.oldrom]
                sr                  b1, [lbs3_bank_rom]
                popa
                ret
.oldrom         db                  0


; blit a 8x8 gfx image bank
; parameters (registers)
;   b0 : gfx image index
;   b1 : gfx image bank
;   w1 : target coordinates on surface 0
blit8x8frombank
                pusha
                mbs3_gfx_setPB1     $81     ; source surface
                mbs3_gfx_setPW2     w0      ; source coordinates
                mbs3_gfx_setPB3     sk_tile_keycolor
                mbs3_gfx_setPB5     0       ; target surface
                mbs3_gfx_setPW6     w1      ; target coordinates
                mbs3_gfx_blitkcolor
                popa
                ret


; transfer rom sk tiles and sprite to gfx surface
transromTileSpriteToGFX
                ; transfer tiles
                mov                 b0, rom_sk_tile_data
                mov                 b1, rom_sk_tile_mask
                mov                 b4, 0 ; tile gfx image bank
                mov                 b2, 0 ; rom img idx 
                mov                 b3, 0 ; gfx img idx 
.transOneTile   c                   trans8x8toGFX
                add                 w1, $0101
                cmp                 b2, 42
                jnz                 .transOneTile
                ; transfer sprite
                mov                 b0, rom_sk_spr_data
                mov                 b1, rom_sk_spr_mask
                mov                 b4, 1 ; tile gfx image bank
                mov                 b2, 0 ; rom img idx 
                mov                 b3, 0 ; gfx img idx 
.transOneSpr    c                   trans8x8toGFX
                add                 w1, $0101
                cmp                 b2, 48
                jnz                 .transOneSpr
                ret


; transfer  to GFX surface number 1
; one 8x8 image with "mask to key color" transform
; to GFX surface number 1
; parameters
;      b0 : rom bank number for data
;      b1 : rom bank number for mask
;      b2 : rom 8x8 image index (0 to 255)
;      b3 : GFX 8x8 image index in image bank
;      b4 : GFX 8x8 image bank number (0: tile, 1: sprite, ... 3)
trans8x8toGFX
                ; save env
                pusha
                ld                  b5, [lbs3_bank_rom]
                sr                  b5, [.oldrom]
                ; set variable
                sr                  b0, [.romdata]
                sr                  b1, [.rommask]
                sr                  b2, [.romimgidx]
                sr                  b3, [.gfximgidx]
                sr                  b4, [.gfximgbank]
                ; prepare computed parameters
                ;   compute rom bank address
                eor                 w0, w0
                ld                  b0, [.romimgidx]
                shl                 w0
                shl                 w0
                shl                 w0
                shl                 w0
                shl                 w0
                add                 w0, rom_scr_addr
                sr                  w0, [.romimgaddr] ; rom addr
                ;   compute GFX surface coordinates
                ld                  b1, [.gfximgbank]
                ld                  b0, [.gfximgidx]
                ;   prepare the mbs3_gfx_blitrans
                mbs3_gfx_setPB1     $81
                mbs3_gfx_setPW2     w0
                mbs3_gfx_blitrans
                cmp                 b0, ebs3_gfx_code_ok ; is it ok ?
                jnz                 .endtrans ; not ok then quit
                ld                  w3, [.romimgaddr]
.transferloop 
                ld                  b0, [lbs3_gfx_status]
                tst                 b0, ebs3_gfx_status_waitdata
                jz                  .endtrans ; end of transfer
                ; get data and mask data from rom
                ld                  b0, [.romdata]
                sr                  b0, [lbs3_bank_rom]
                ld                  b2, [w3]
                ld                  b0, [.rommask]
                sr                  b0, [lbs3_bank_rom]
                ld                  b3, [w3]
                ; b2 = data, b3 = mask for 2 pixels 
                ; (1 byte = 2 pixels. MSq:first, LSq: second)
                ;  b4 = 1st pixel
                mov                 b4, b2
                shr                 b4
                shr                 b4
                shr                 b4
                shr                 b4
                ; first pixel : b3 4bits mask, b4 4bits data
                tst                 b3, $F0 ; 1st pixel mask
                jz                  .plot1st
                cmp                 b4, 0
                jnz                 .plot1st
                mov                 b4, sk_tile_keycolor
                ; transfer first pixel
.plot1st        mbs3_gfx_setPB3     b4
                call                lbs3_gfx_busywait
                ; b2 = 2nd pixel
                and                 b2, $0F
                tst                 b3, $0F ; 2nd pixel mask
                jz                  .plot2nd
                cmp                 b2, 0
                jnz                 .plot2nd
                mov                 b2, sk_tile_keycolor
                ; transfer second pixel
.plot2nd        mbs3_gfx_setPB3     b2
                call                lbs3_gfx_busywait                
                inc                 w3
                j                   .transferloop
                ; restore env
.endtrans       ld                  b5, [.oldrom]
                sr                  b5, [lbs3_bank_rom]
                popa
                ret
.romdata        db                  0
.rommask        db                  0
.romimgidx      db                  0
.gfximgidx      db                  0
.gfximgbank     db                  0                
.oldrom         db                  0
                align               2
.surfcoords     dw                  0 
.romimgaddr     dw                  0 

; sk data
sk_data         align               16

; end of sk.asm                