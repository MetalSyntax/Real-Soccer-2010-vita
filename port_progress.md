# Real Soccer 2010 — Progreso del port (iOS → PS Vita)

## 2026-11-04 — Inicialización y runtime completo (todo offline; falta solo probar en consola)

### Hecho
- `psvita-toolkit init --ipa "IPA/Real Soccer 2010_v.1.1.0.ipa" --name "Real Soccer 2010" --titleid PSVRS0010 --slug realsoccer10 --vita-ip 192.168.3.15` (commit del scaffold).
- Binario `RSoccer10`: armv6 (ARM), cryptid 0 (descifrado). MinimumOSVersion 2.2.1.
- Motor: Gameloft C++ propio (sucesor de RS09; misma carcasa ObjC iRF09AppDelegate/EAGLView) + GameKit (multijugador local, stub).
- Arranque: `MainWindow.nib` igual que RS09 (precompilado a `nib.txt`).
- **El boilerplate iOS que copia el toolkit no compila ni tiene runtime ObjC** → se reemplazó por el loader probado de Inotia2-vita + un runtime nuevo y común a los 6 ports iOS (copia maestra en `Ferrari-GT-Evolution-vita/source`, se sincroniza con `tools/sync_runtime.sh <port>`). Detalle en `PORTING_PLAN.md` §1.
- `tools/ios_prep.py` → `rebase.bin` **29217** palabras, `binds.bin` **1488** slots / 417 símbolos. Modo: clásico sin relocs (heurístico + estructural). Cobertura de `__text`: 96.0%; 16591 literales puntero, 4078 palabras de tablas de salto; 5159 candidatos rechazados (líneas `DUDA` de `report.txt`).
  - El clasificador se validó contra HAWX (que trae símbolos): 0 punteros perdidos, 0 falsos positivos en datos.
- Cobertura de imports: todos resuelven (implementación, clase vacía autogenerada, NSString constante sintetizada o stub `unimpl` que lo registra en el log).
- C++: `tools/gen_cxx.py` mapea la libstdc++ de iOS (ABI COW) a la de VitaSDK (`source/cxx_imports.c`).
- Audio: OpenAL (1574 `.wav`) + música `AudioFile`/`AudioQueue` (4 `.m4a` → `.ogg`).
- Video: `REAL-SOCCER2010-COMP-FINAL.m4v` → se saltea.
- Controles: Igual que RS09 (táctil) → `controls.txt`.
- Rotación del drawable: 1 (Info.plist: LandscapeRight).
- Build OK: `psvita-toolkit build --preset debug` → `build/realsoccer10.vpk`.
- Datos preparados: `tools/stage_data.sh` → `ux0_data/realsoccer10/` (bundle, `rebase.bin`, `binds.bin`, `nib.txt`, `controls.txt`, música `.ogg`).

### Para probar en consola (Vita 192.168.3.15, VitaShell FTP en 1337)
1. Instalar `build/realsoccer10.vpk`.
2. `python3 -I tools/deploy_data.py` (sube `ux0_data/realsoccer10/` → `ux0:data/realsoccer10/`).
3. Requisitos en la consola: `kubridge.skprx` en `ur0:tai/config.txt` y `libshacccg.suprx` (ShaRKBR33D).
4. Abrir el juego; si se cierra o queda en negro: `python3 -I tools/deploy_data.py --logs` y leer `logs/realsoccer10_NNN.log` (+ `.psp2dmp`).
5. Rotación incorrecta: mantener SELECT+START 2 s (cicla y guarda `rotation.txt`).
6. Botones: editar `ux0:data/realsoccer10/controls.txt` por FTP (coordenadas de lo que se ve), sin recompilar.

### Riesgos conocidos
- GameKit/Bluetooth: clases vacías (sin multijugador).

### Revisión independiente del runtime (misma sesión, antes de la consola)
Un revisor aparte leyó asm.S/objc.c/macho.c. Se corrigió:
- **Crítico:** las cadenas ObjC (selectores, nombres de clase, UTF-16) viven en `__TEXT` y se leían antes de `macho_finalize()` (el bloque RX todavía estaba vacío). Ahora `foundation_fix_cfstrings` + `objc_init_image` corren después; las escrituras en `__TEXT` van por `macho_write_word` (kubridge).
- Pool de autorelease por hilo (TLS); antes era uno solo compartido → use-after-free con hilos/GCD.
- Caché de métodos sin carrera (cls=0 → imp/sel → cls, con barreras y re-chequeo).
- `-dealloc` no puede correr dos veces (rc centinela durante el dealloc).
- `methodForSelector:` devuelve un thunk con `ml_align_call` para IMPs nativas; `objc_ivar_set` libera el valor anterior.
- Pendiente (latente, sin uso en estos binarios): `+initialize`/`+load` no se envían; `nil_imp` en llamadas `_stret` deja el struct sin tocar.
- Verificado correcto por el revisor: balance de pila/registros de todos los trampolines, `ml_dispatch_imp` (Thumb incluido), `objc_msgSendSuper2`, setjmp, cadena de metaclases, mapeo y thunks de macho.c, firmas nativas con CGRect/NSRange/double.

### Corrida 001 (consola) — crash en mod_init[2]
- Data abort en PC 0x23278c (stub de `_memset`), r12 = 0x26073c sin rebasar, llamado desde `CGLFont::CGLFont` (0x2178).
- Causa: `__symbol_stub4` de este binario son de 12 bytes (`ldr ip,[pc]; ldr pc,[ip]; .long lazy_ptr`); `ios_prep.py` solo rebasaba la variante de 8 bytes.
- Fix en `tools/ios_prep.py` (y en la copia maestra de Ferrari): rebasa la última palabra de cada stub para tamaños 8 y 12. rebase.bin: +324 palabras. Subido.

### Corrida 002 (consola) — bad_alloc en `new CGlobal`
- Llega a main del juego / UIApplicationMain / EAGLView; aborta en `operator new` (bad_alloc) desde 0xca000.
- El literal 0xca0f4 = 0x7644 (sizeof CGlobal) se rebaseaba → `new(0x98007644)`. La copia local de `ios_prep.py`
  estaba vieja: marcaba como "inicio débil de función" lo que sigue a CUALQUIER instrucción incondicional (0x7640 `mvn`).
- Se sincronizó con la maestra (solo tras salto/retorno incondicional) y se agregó en ambas: lo que sigue a un `bl`
  también cuenta (landing pads SjLj tras `bl ___cxa_rethrow`, ej. 0x17fbd8; la maestra los perdía).
- rebase.bin: 29543 → 29373 (169 falsos positivos fuera: 10000, enums GL 0x2601/0x2801, tamaños). Subido.
- Pendiente visto en el log: `-[UIActivityIndicatorView initWithActivityIndicatorStyle:]` sin implementar.

### Corrida 003 (consola) — bad_alloc en `new CFMath` (dentro de CGlobal::CGlobal, 0x93d94)
- Literal 0x93fb8 = 0x4010 (sizeof CFMath) rebaseado: sigue a `bl _glPushMatrix` y la regla "tras bl" de la corrida 002
  lo aceptaba como argumento.
- Fix (local + maestra): lo que sigue a un `bl` va a `scan.after_bl` y solo se rebasea si el literal se GUARDA
  (`use == 'store'`, como el landing pad SjLj); como argumento casi siempre es un tamaño. Salen también 0x4004/0x4005
  (GL_LIGHTx) y 0x8D40 (GL_FRAMEBUFFER_OES). Sincronizado con la maestra (trae `numeric_table`). rebase.bin: 29286. Subido.

### Corrida 004 (consola) — bad_alloc en `new` de 0x23eb4 (tras abrir res.pak / res_db.pak)
- Literal 0x24418 = 0x1225c (tamaño) rebaseado: sigue a un `b` incondicional → "inicio débil" + uso 'arg'.
- Fix (local + maestra): `CodeScan.alloc_stubs` (stubs de `_Znwm/_Znam/_Znwj/_Znaj/malloc/calloc/realloc`); un literal
  que va a un `bl` a esos stubs se clasifica 'int' (es un tamaño). Sale también 0x81650. rebase.bin: 29284. Subido.

### Corrida 005 (consola) — pantalla negra y sin sonido (sin crash; 207 frames / 15596 draw calls)
- **Video:** `glEnableClientState(0x8074 GL_VERTEX_ARRAY / 0x8075 GL_NORMAL_ARRAY)` llegaba rebaseado (0x98008074),
  así que vitaGL nunca activaba los arrays y no se dibujaba nada. Fix en `ios_prep.py` (local + maestra): los literales
  pasados a stubs `_gl*`/`_al*` se clasifican 'int' (junto a los asignadores). rebase.bin: 29260 (−25, solo esos).
- **Sonido 1:** `alGetError()` sin contexto → 0xA004 en openal-soft ("Error opening output device: a004"); el juego
  aborta el audio. Wrapper `al_GetError` (la maestra ya lo tenía).
- **Sonido 2:** `AudioFileOpenURL` recibía la ruta iOS `/var/mobile/.../RSoccer10.app/x.m4a` sin traducir → no
  encontraba el .ogg. La maestra ya lo resolvía con `darwin_path`, y además trae AudioFileStream (SFX WAV de sounds.bar).
- Runtime sincronizado desde Ferrari (`sync_runtime.sh`): el local era del 04/11. eboot y rebase.bin subidos.
- Pendientes vistos: `-[UIActivityIndicatorView initWithActivityIndicatorStyle:]`, `+[MPMediaQuery songsQuery]`,
  `-[NSString isAbsolutePath]`, `NSDateFormatter`, `-[NSData initWithContentsOfURL:]` (IGP, sin red: inofensivo).

### Corrida 006 (consola) — ¡se ve! title/menú al revés; crash al tocar "Amistoso"
- **Al revés:** `-[UIDevice orientation]` devolvía 4 (home a la izquierda) con rotación 1, que muestra home a la
  derecha (como el acelerómetro simulado y el Info.plist: UIInterfaceOrientationLandscapeRight). El juego sigue la
  orientación del dispositivo (`setStatusBarOrientation:4`) y giraba su contenido 180°. Fix en uikit.c (común):
  rotación 1 → 3, rotación 2 → 4.
- **Música:** `AudioFileGetPropertyInfo` decía noErr para todo; BackgroundTrackMgr (Apple) pedía 'cmap', fallaba
  ("Error setting up queue: pty?") → sin música. Ahora GetPropertyInfo responde lo mismo que GetProperty.
- **Crash** en `af_Close(0x30002)` desde `BackgroundTrackMgr::Teardown` (0xb36c0): BG_FileInfo liberado tras el
  fallo de SetupQueue anterior (mismo origen 'cmap'); debería desaparecer con el fix de arriba.

### Corrida 007 (consola) — menús, música y SFX OK; crash al entrar al partido
- Data abort en 0x24e8c (`CAnimationManager::ReleaseMeshAndTex`, desde 0x2ce10): `delete` virtual sobre
  `this+0x39e50` = basura. El ctor (0x2862c) inicializa +0xe4c pero nunca +0xe50; en iOS el objeto (238 KB) sale de
  páginas nuevas en cero. Fix en darwin.c (común): `__Znwm/__Znam` → `d_new` (calloc). `KeyFrameInfo.dat` falta
  también en el IPA (opcional, el juego sigue).

### Corrida 009 (consola) — crash en `CAnimationManager::InitializeLight` (0x2ca80)
- `str r2, [r0, r3]` con r3 = literal 0x2cb44 = 0x39d04 (offset de campo) rebaseado: 0x39d04 coincide con un inicio
  de función ("fuerte") y el uso 'arith' lo aceptaba.
- Fix en `ios_prep.py` (local + maestra): uso nuevo 'regadd' (índice de memoria `[rB, rX]` o add/sub con dos
  registros) = desplazamiento → no se rebasea si cae en __text (en datos sigue como 'arith': base de tabla + índice).
  Sale también 0x3dd8. Tomada además la lista de enums GL por puntero de la maestra. rebase.bin: 29256. Subido.

### Corrida 010 (consola) — llega al partido: suena, pero negro y ~10-15 fps (≈200 draws/frame)
- Todas las gl* que usa el juego están implementadas; fog/luces/frustum en punto fijo de vitaGL verificados en su
  fuente; depth/stencil del FBO de pantalla soportado. Sin GLERR en el log. Causa del negro todavía sin evidencia.
- **Rendimiento:** el vitaGL vendorizado SÍ acepta GL_FIXED nativo en vértice/normal/texcoord (shader ffp_v:
  GLFixedToFloat). gles.c (común) ya no convierte en CPU salvo el color (y el skinning); `glDrawElements` deja de
  recorrer los índices; `gles_present` restaura también los punteros fijos nativos.
- **Diagnóstico:** gles.c de la maestra (TRACE de un frame + GLERR por llamada) + `PORT_TRACE_EVERY 300` en
  port_config.h (traza 8 frames más, cada 300) y 12 capturas (cada 10 s) para ver el partido. Quitar al verse.

### Corrida 011 (consola) — cuelgue de GPU al empezar el partido (sin dump), 3 fps por el trazado
- TRACE del frame 600 (partido): modelview con `M[5] = 5.9e20` (imposible desde punto fijo) → matrices corruptas.
- Causa: vitaGL tiene pilas de proyección/textura de 2 niveles (contador arranca en 1 → un solo push) y se compila
  con `-DSKIP_ERROR_HANDLING`: un push de más escribe fuera de `projection_matrix_stack` y pisa
  projection/modelview/mvp. `gles_present` empujaba proyección+textura cada frame (encima de lo del juego).
- Fix en gles.c (común): `gles_present` guarda/restaura matrices con glGetFloatv/glLoadMatrixf; `g_glPush/PopMatrix`
  llevan pila propia de 16 niveles para proyección y textura (modelview sigue en vitaGL, 32).
- GL_FIXED nativo revertido (prudencia: el cuelgue apareció con él; probarlo aparte cuando el partido se vea).
- GLERR 0x500 tras glLightfv/glMaterialfv: residuo de la llamada directa anterior (glLightxv/glMaterialx), no causa.
- Trazado periódico apagado.

### Corrida 013 (consola) — sin cuelgue de GPU, pero el partido sigue negro
- El TRACE del frame 450 cayó en 2D (menú/carga); no hay evidencia del 3D. FTP apagado: sin capturas nuevas.
- Diagnóstico nuevo en gles.c (común): el primer `glEnable(GL_LIGHTING)` arma el trazado del frame siguiente y otro
  150 frames después (hasta 120 draws), con P/M completas, depth range/func/mask, colormask, clear y fog de los
  primeros 6 draws 3D, y captura `logs/frame_lit_N.tga` del FBO al terminar ese frame.
- gles.c igualado con la maestra (rotación 0 con barra de estado en retrato); de uikit.c solo se tomó
  `ui_status_portrait` (el hit-testing táctil nuevo de la maestra queda fuera hasta probarlo).

### Corridas 014-015 — los trazados cayeron en menús
- `glEnable(GL_LIGHTING)` ya ocurre en el menú sin draws 3D: los frames trazados (174, 324, 398, 450) eran el menú
  "Empezar partido" (frame_lit_*.tga). `KeyFrameInfo.dat` se abre al entrar a ese menú, no al partido.
- Disparador nuevo (`lit_check`): primer draw con GL_LIGHTING + proyección en perspectiva (P[11] = -1); traza ese
  frame y 2 más (+150, +300). Tomado el perfilador por frame de la maestra (línea `PERF` cada 5 s).

### Corrida 016 (consola) — el partido NO es un problema de render: el hilo principal se congela tras el saque
- Capturas frame_lit_*: el 3D iluminado de los menús (camisetas en "Elegir camiseta") se ve bien → luces, matrices y
  profundidad OK. El log se corta 1-8 s después de `vfx_kickoff.wav` en TODAS las corridas (010-016): sin PERF ni
  `vivo` (el watchdog del run loop tampoco salta porque corre en el mismo hilo trabado).
- PERF (perfilador de la maestra): GL ≈ 2-4 ms/frame; el resto (juego + runtime + log) 40-290 ms.
- **Causa probable (igual que Real Soccer 2009, corrida 011):** `GlRenderer::Init` (0x14dc0) hace 100 glPushMatrix +
  100 glPopMatrix en modelview; vitaGL (SKIP_ERROR_HANDLING, modelview 32, contador uint8) pisa sus globales.
  Fix en gles.c (común): pilas propias de 128 niveles para modelview, proyección y textura.
- Watchdog movido a un hilo propio (uikit.c común): 8 s sin presentar → crash deliberado con dump de todos los hilos.
- `NSTimer` del log limitado a 30 líneas (el juego programa uno por frame: 2 escrituras a ux0 por frame).
- GL_FIXED nativo otra vez (maestra): el "cuelgue de GPU" de 011 era este mismo congelamiento.
- Pendiente de Real Soccer 2009 aplicable acá: openal.c (alSourceStop antes de unqueue en pausa), audio.c (ov_open +
  setvbuf 64 KB), input.c `STICK` para el pad táctil del partido.

### Corrida 017 (consola) — watchdog disparado en el menú: falso positivo del diagnóstico
- Dump de todos los hilos (vita-parse-core, `~/vita-tools/vita-parse-core`): hilo principal en `sceIoWrite` ←
  `fwrite` ← `dump_frame` (captura TGA de 600 KB) ← `gles_present`. Dos trazados seguidos (frames 440 y 450) y la
  escritura superó los 8 s del watchdog. El partido no llegó a probarse con el fix de la pila modelview.
- Fix: macro `PORT_GL_DIAG` en gles.c (común, por defecto 1); este port `PORT_GL_DIAG 0` → sin capturas ni trazados.

### Corrida 018 (consola) — ¡partido jugable! ~28-31 fps; destellos y cambios de color bruscos
- Confirmado: el congelamiento era el desborde de la pila modelview de vitaGL (fix de la corrida 016).
- Gráficos: Real Soccer 2009 (limpio) convierte GL_FIXED en CPU; los destellos aparecieron con el nativo de vitaGL.
  Macro `PORT_NATIVE_FIXED` en gles.c (común, por defecto 1); este port `PORT_NATIVE_FIXED 0` (conversión < 2 ms/frame).
- GLERR de luces: vitaGL no implementa GL_SPOT_* (glLightf/glLightfv de focos → INVALID_ENUM, se ignoran). Pendiente.
- Controles como Real Soccer 2009: `STICK x y r` (input.c, ahora también en la maestra; opt-in) + MAX_TOUCH 9 (uikit.c).
  Zonas del partido leídas de `CTouchScreen::GetTouchType` (tabla en 0x250160): pad = mitad izquierda, A (330,272),
  B (430,272), arriba-derecha (404,20)/(450,20). controls.txt: CROSS=B, SQUARE=A, CIRCLE=atrás, TRIANGLE/R=arriba-der.,
  START=centro, STICK 80 240 55.
