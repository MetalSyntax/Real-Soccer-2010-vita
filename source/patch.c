// Parches de este juego. Vacío hasta la primera corrida en consola: cada parche se agrega con
// evidencia (log/dump + desensamblado con tools/annotate.py) y su dirección del Mach-O.
#include <stdio.h>

#include "hle.h"
#include "imports.h"
#include "log.h"
#include "macho.h"
#include "patch.h"

const import_entry patch_imports[] = {
    {"", 0, 0},
};
TABLE_END(patch)

void patch_init(void) {
    // CFootBall::SetBallOutTo_Reflect (0x39d04): solo corta la trayectoria a 299 cuadros si [sp+0xc] != 0; con
    // velocidad alta (43285, desacel. 51 → 850 cuadros, corrida 019) desborda el arreglo de 300 en ball+0x68 y pisa
    // capture (+0x1654) → crash en CFormationPool::Update. Tope siempre: nop del beq y "bne" → "blt" (r11 >= 299).
    static const uint32_t nop = 0xe1a00000, blt = 0xba000001; // blt 0x3a19c
    if (macho_read_text(0x3a184) == 0x0a000004 && macho_read_text(0x3a190) == 0x1a000001) {
        macho_patch_text(0x3a184, &nop, 4);
        macho_patch_text(0x3a190, &blt, 4);
    } else
        l_error("patch: SetBallOutTo_Reflect no coincide, sin parche");

    // "Más juegos" (IGP de Gameloft): sin red ni catálogo, gira la pantalla a vertical y queda sin salida (corrida 030,
    // se entra con CROSS al pasar rápido por el menú). CMainMenu/CNewMainMenu::GoNext llaman SetGameState(9) (CGSIGP):
    // nop → la opción no hace nada.
    static const uint32_t igp_at[] = {0x1bc58c, 0x1dbb6c}, igp_bl[] = {0xebfb52df, 0xebfad567};
    for (int i = 0; i < 2; i++)
        if (macho_read_text(igp_at[i]) == igp_bl[i])
            macho_patch_text(igp_at[i], &nop, 4);
        else
            l_error("patch: SetGameState(IGP) en 0x%x no coincide, sin parche", igp_at[i]);

    // ponytail: bisección corrida 025 — gldbg.txt bit 16 anula CPlayGround::RenderCameraBigShadow (0x94240) para ver
    // si la zona negra del partido es la sombra mal mezclada o césped/estadio sin dibujar. Quitar al cerrar el bug.
    int dbg = 0;
    FILE *f = fopen(DATA_PATH "gldbg.txt", "r");
    if (f) {
        if (fscanf(f, "%i", &dbg) != 1)
            dbg = 0;
        fclose(f);
    }
    static const uint32_t bx_lr = 0xe12fff1e;
    if ((dbg & 16) && macho_read_text(0x94240) == 0xe92d40b0) {
        macho_patch_text(0x94240, &bx_lr, 4);
        l_info("patch: RenderCameraBigShadow anulado (gldbg 16)");
    }
}
