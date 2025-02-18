! Sega Saturn USB flash cart ROM
! by Anders Montonen, 2012
! Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)

! System ID block

.section .sysid

hwid:       .ascii "SEGA SEGASATURN "
makerid:    .ascii "TPU             "
produm:     .ascii "T-7605781 "
version:    .ascii "V1.003"
reldat:     .ascii "20230214"
devinfo:    .ascii "R  2    "
area:       .ascii "JTUEABLK        "
periph:     .ascii "J               "
title:      .ascii "SAROO firm      "
            .ascii "                "
            .ascii "                "
            .ascii "                "
            .ascii "                "
            .ascii "                "
            .ascii "                "
            .ascii "                "
!ipsize:     .long  __ip_len
ipsize:     .long  0x4000
stackm:     .long 0
            .long 0
stacks:     .long 0
firstaddr:  .long 0
firstsize:  .long 0
            .long 0
            .long 0

            .incbin "sega/sys_sec.bin"
            .incbin "sega/sys_arej.bin"
            .incbin "sega/sys_aret.bin"
            .incbin "sega/sys_areu.bin"
            .incbin "sega/sys_aree.bin"
            .incbin "sega/sys_area.bin"
            .incbin "sega/sys_areb.bin"
            .incbin "sega/sys_arel.bin"
            .incbin "sega/sys_arek.bin"
