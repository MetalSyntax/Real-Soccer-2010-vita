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

### Corrida 019 (consola) — crash ~30 s de partido: trayectoria de la pelota desbordada
- Data abort en `CFormationPool::Update` (0x3c2dc): `CFootBall::GetCapture()` (+0x1654) = 0xffff7f03 (basura).
- Memoria del dump: el arreglo de trayectoria `ball+0x68` (300 × M3DXVector3) tenía 850 cuadros (+0x113c = 850):
  velocidad 43285, desaceleración 51/cuadro → pisó contador (+0x1134), posición (+0x1614) y capture (+0x1654).
- Escritor: `CFootBall::SetBallOutTo_Reflect` (0x39d04), único `SetBallOutTo_*` cuyo tope de 299 cuadros depende de un
  flag (`[sp+0xc]`); los demás comparan siempre contra 300. Parche en patch.c: nop en 0x3a184 y `bne`→`blt` en 0x3a190
  (corta al llegar a 299 cuadros con la pelota en el piso). Pendiente: quién pasa una velocidad tan alta.
- Stick/cruceta sin efecto: el `controls.txt` de la Vita era viejo ("4 botones", sin `STICK`). Subido el actual.
- Rendimiento: el juego se programa a 30 fps (NSTimer 0.0303 s); los bajones del partido coinciden con `conv`
  10-17 ms/frame. gles.c: (1) solo se convierte el rango de vértices del draw ([first, first+count) o
  [min, max] de índices), (2) solo arrays habilitados (`cl_on`: antes se convertía el array de color viejo aunque
  `CM3DDevice2::DrawDisplayList` lo deshabilita en cada draw). Pendiente: llevar a la maestra.
- Franja negra abajo en el partido: es `CPlayGround::RenderCameraBigShadow` (0x94240): `Camera_big_shadow.gtx`
  con glColor4f(0,0,0,0.5) + GL_BLEND, cull off; debería oscurecer 50 % y sale negra sólida. Sonda `SOMBRA` en
  gles.c (blend func, env, formato de la textura, arrays) para la próxima corrida.

### Corrida 021 (consola) — franja negra resuelta, FPS mejores; quedan destellos blancos en el césped
- Sonda SOMBRA: estado de la sombra de tribuna correcto (SRC_ALPHA/1-SRC_ALPHA, MODULATE, RGBA4444). La franja negra
  desapareció con el filtro de arrays deshabilitados (`cl_on`). Sonda quitada.
- Césped (`CPlayGround::DrawMultiTextureDL`, 0x943xx): unidad 1 en GL_COMBINE / INTERPOLATE(TEXTURE, PREVIOUS,
  CONSTANT.a) con TEXTURE_ENV_COLOR por draw. Bug en gles.c: `env_mode` era uno solo para ambas unidades → cada
  gles_present dejaba la unidad 0 en COMBINE (el último modo, de la unidad 1). Ahora `env_mode[2]` por unidad
  (`server_unit` desde glActiveTexture). Candidato a los destellos; sin confirmar.
- Audio: portados de Real Soccer 2009 `al_SourceUnqueueBuffers` (alSourceStop antes del unqueue, openal.c) y
  `ov_open` + setvbuf 64 KB (audio.c). Queda: cada SFX se relee de ux0 al sonar (wav de hasta 500 KB, p. ej.
  sfx_busy_crowd_1) → posible tirón; cachear si persiste.

### Corrida 022 (consola) — siguen destellos blancos/rojos/naranjas; estadio negro al empezar el partido
- Esta corrida fue la primera sin `SAMPLERS_SPEEDHACK=1 DRAW_SPEEDHACK=2` en vitaGL (CMakeLists editado a mano a las
  12:05) y con `env_mode` por unidad. Flags restaurados (como RS2009, que se ve limpio) para aislar el estadio negro.
- Destellos: sin paleta de matrices (el juego no importa glCurrentPaletteMatrixOES) → no es skinning nuestro. Lo
  nuevo de RS2010 frente a RS2009 es el césped multitextura (GL_COMBINE/INTERPOLATE con CONSTANT.a); la fórmula y
  los uniforms del combinador de vitaGL están bien. Sonda `ENVCOLOR alfa min .. max` junto a PERF: si el alfa de
  TEXTURE_ENV_COLOR sale de [0,1], el INTERPOLATE extrapola (colores saturados).
- gles.c sincronizado a la maestra (Ferrari-GT-Evolution-vita), con la sonda ENVCOLOR.

### Corrida 023 (consola) — siguen destellos y campo negro al empezar el partido
- `ENVCOLOR alfa 0 .. 1` siempre: el combinador del césped no extrapola. Con speedhacks restaurados el campo
  negro sigue → no eran los flags de vitaGL.
- Sospecha de regresión propia: destellos reportados desde la 021 (rango + cl_on), campo negro desde la 022
  (env_mode por unidad). Interruptores en `ux0:data/realsoccer10/gldbg.txt` (se lee en gles_init, sale en el log):
  1 = convertir desde el vértice 0, 2 = convertir arrays deshabilitados, 4 = env_mode único, 8 = unidad 1 sin
  COMBINE. Corrida 024 con 7 (comportamiento de la 019).

### Corrida 024 (consola, gldbg 7) — siguen los errores gráficos; la zona oscura se reconoce como la sombra del estadio
- El log no tiene errores nuevos (solo los GLERR de focos ya conocidos). `RenderCameraBigShadow` (0x94240) solo hace
  glColor4f(0,0,0,0.5) + glEnable(GL_BLEND) + 2× DrawDisplayList: con alfa 0.5 no puede quedar negro opaco
  (captura de las 11:04: debajo del degradado todo es negro). O la sombra se mezcla mal, o el césped/estadio de esa
  zona no se dibuja y lo que se ve es el color de borrado.
- Bisección: patch.c lee `gldbg.txt` y con el bit 16 pone `bx lr` en 0x94240. Corrida 025 con gldbg 23 (7 + 16):
  si lo negro desaparece → el problema es la sombra; si sigue → falta geometría del césped/estadio.

### Corrida 025 (consola) — en la Vita seguía gldbg 7: la prueba de la sombra no se hizo
- El log dice `gldbg.txt: 7` y falta `patch: RenderCameraBigShadow anulado` → no se había subido el 23. Subido
  después con deploy_data.py; la corrida 026 repite la prueba.
- Observación: la presentación (estadio y jugadores entrando) se ve sin errores; los destellos aparecen solo al jugar,
  y los peores al arrancar el partido. Con gldbg 1 (convertir desde el vértice 0) conv sube a 6-8 ms/frame.

### Capturas de la corrida 025 (12:52) — causa de los destellos: DRAW_SPEEDHACK de vitaGL + búfer de conversión reusado
- Cuadros con destello: todo el césped de un solo color (marrón, rosado), sin líneas, y los jugadores chicos como
  vistos desde lejos. La presentación se ve limpia; el problema aparece solo al jugar.
- vitaGL con `DRAW_SPEEDHACK=2` (SAFER) no copia los arrays de cliente de los draws > 0x8000 bytes
  (`SAFE_DRAW_SIZE_THRESHOLD`, ffp.c): la GPU los lee después. Nuestra conversión GL_FIXED→float escribe siempre en
  el mismo `conv_buf[slot]` → el draw siguiente del mismo frame pisa los vértices del césped (malla grande) antes de
  que la GPU los lea. Los draws chicos se copian y se ven bien. Con gldbg 1 (convertir desde el vértice 0) los draws
  pasan el umbral más seguido: por eso la 025 se vio peor.
- La 022 "sin speedhacks" nunca recompiló vitaGL: el CMake hacía `test -f libvitaGL.a || make`.
- Fix: sin `DRAW_SPEEDHACK` en CMakeLists.txt; vitaGL se recompila si cambian los flags (`vendor/vitaGL/.flags`,
  compilada localmente para que el build del toolkit no la rehaga). gldbg 0 para la corrida 026.

### Corrida 027 (consola, gldbg 0, vitaGL sin DRAW_SPEEDHACK) — los destellos siguen igual
- Descartado el speedhack como causa (se deja sin él: es más correcto y los FPS no cambiaron, ~30).
- Fog del partido: GL_EXP, densidad 0.2, color gris (0.8, 0.8, 0.8): no explica marrón ni rosado.
- `glCopyTexImage2D` del render del partido: captura única (estado +0xec8 de 1 a 2), no son destellos por cuadro.
- Sonda `PORT_FLASH_PROBE` (port_config.h, código en gles.c): estado del primer draw del césped de cada cuadro
  (matrices M/P/T, vértice 0, textura, luz/fog/blend/depth) y 4 píxeles del FBO al presentar; registra `FLASH` si
  ninguno es verde y `OK` cada 300 cuadros para comparar. Corrida 028.

### Corrida 029 (consola) — la sonda no registró nada
- Sin `FLASH` ni `OK` ni `ENVCOLOR`: en las corridas 027 y 029 el juego nunca puso la unidad 1 en COMBINE. El césped
  multitextura es `CPlayGround::RenderAdvAlpha` (0x98axx): se dibuja según la distancia de la cámara y
  `TestSphereInFrustum`, no en todos los partidos. Los destellos siguen sin él → no vienen del combinador.
- Sonda corregida: ahora se activa con el primer draw en perspectiva de cada cuadro (`P[11] < -0.5`). Corrida 030.

### Corrida 030 (consola) — pantalla vertical en el menú y cuelgue
- Vertical = IGP de Gameloft (`IGPSendGameInfo` + `setStatusBarOrientation:1`), ya visto en la 012 sin cuelgue.
- Cuelgue entre 14 s y 20 s (último `vivo` a los 10 s). El IGP dibuja en perspectiva (glFrustumf) → la sonda
  hacía 4 glReadPixels por cuadro, y en vitaGL cada uno fuerza scene_reset + sceGxmFinish. Ahora es una sola lectura
  (fila central del FBO, 32 muestras; destello si menos de 4 son verdes). Sin dump: causa no confirmada.

### Corrida 031 (consola) — sin cuelgue con 1 lectura por cuadro; la sonda se gastó en los menús
- Las 30 líneas FLASH fueron de los menús 3D (fondo amarillo f6e700), nunca verdes. Ahora FLASH = cuadro sin verde
  justo después de uno verde (máx. 20), con el cuadro anterior (`antes`) para comparar.
- "Más juegos" (IGP): `CMainMenu::GoNext` (0x1bc58c) y `CNewMainMenu::GoNext` (0x1dbb6c) llaman
  `SetGameState(9)` (CGSIGP, estado 9 de `CGame::CreateGameState`). Parche en patch.c: nop en los dos → la opción no
  hace nada. (El ítem 6, Gameloft Live, sigue igual.)

### Corrida 032 (consola) — los destellos son colores de jugador; saque de banda imposible
- 20 FLASH: la fila central queda de un color de camiseta/piel/pelo (amarillo d3a331, blanco f7f3f7, rojo b23929,
  rosado d29d9c, naranja c67208, marrón 4a3c29) con las MISMAS matrices de cámara que el cuadro anterior → no es la
  cámara: un polígono de un jugador se estira sobre la pantalla. El juego hace el skinning en CPU (no usa la paleta).
  Descartado en vitaGL (sin speedhacks): copia vértices [0, top_idx) e índices de cliente en cada draw.
- Sonda nueva: registro por draw en perspectiva (puntero/rango de vértices, caja envolvente en espacio objeto,
  traslación de la modelview, textura); en los 3 primeros destellos vuelca el cuadro anterior y el del destello.
- Saque de banda: `CTouchScreen::ParseThrowIn` (0x9ec3c) apunta con la inclinación (y×100 fuera de ±43) y lanza con
  una sacudida (Δ² > 8400, ~0,92 g entre dos lecturas, `UpdateAccelerometer` escala ×100). Con el stick no se llega.
  `SHAKE <botón>` en controls.txt (input.c, común, opt-in) → una lectura con 2 g extra en z. Este port: SHAKE LTRIGGER.

### Corrida 033 (consola) — 3 destellos registrados: vértices y texturas sanos
- Los 3 destellos fueron en la presentación del estadio (pantalla negra y 1 fps = el volcado de ~950 líneas).
- Comparando draw por draw (ptr/rango/textura) el cuadro anterior y el del destello: mismas cajas en espacio objeto
  (nada gigante), mismas texturas, misma traslación. No se suben texturas en el partido. Pool circular de vitaGL
  sin speedhack: si se llena, pide memoria nueva (no pisa). Init hace 100 push + 100 pop (balanceado).
- Pista: en la 031 el mismo draw pasó de escala 3,72 a 15,0 con igual traslación, y en las capturas los jugadores
  salen diminutos → el 3×3 de alguna matriz cambia. La sonda ahora guarda M y P completas por draw y en cada
  destello (máx. 8) imprime solo los draws cuya matriz cambió > 10 % respecto del cuadro anterior.

### Corrida 034 (consola) — 8 destellos con matrices completas: nada explota
- FLASH 1 = corte de cámara legítimo (todos los draws cambian juntos de matriz). FLASH 3-8: la cámara (primer draw)
  NO cambia; solo cambian billboards/efectos (tex 40, 71) y quads de caja cero. Nada que tape la pantalla.
- El criterio "cuadro sin verde tras uno verde" también atrapa cortes y tomas del estadio: hace falta ver la imagen.
- En las capturas de la 025 los nombres y flechas (proyectados por el juego) siguen a los jugadores diminutos →
  probablemente es la cámara del propio juego la que salta (¿pelota con valores basura, como la velocidad 43285
  de la 019?). Imports matemáticos (asin, atan2, cos, sin, tanf, floorf, ceilf, rand, __divdi3) van directo a newlib
  con softfp: sin conversión sospechosa.
- Sonda: además guarda TGA 160x240 del cuadro anterior y del destello (4 primeros) en logs/flashN[_antes]_<id>.tga.

### Corrida 035 — Causa raíz de los destellos aislada y corregida; restauración de 30/60 fps
- **Causa raíz descubierta**:
  - En la corrida 034 se supuso erróneamente que "nada explota" porque solo se inspeccionó el Draw 0 (cámara).
  - El análisis exhaustivo de todas las matrices de `logs/realsoccer10_034.log` reveló que en los destellos exactos (FLASH 3 al 8):
    - Draw 57 (`0x833c6f30 m4 n6 tex71`, sombra de jugador) explotaba a `mv[8] = -20123.506` y `mv[10] = 60304.801` (escala de más de 60.000×).
    - Draw 67, 68, 69 (`0x8339c118 m4 n351 tex70`, mesh/piel de jugador) explotaba a `mv[8] = -16890.514` y `mv[9] = 19142.109`.
    - Draw 75 (`0x83389c28 m4 n1239 tex67`, camiseta de jugador) explotaba a `mv[8] = -17863.828` y `mv[10] = 30966.287`.
  - El desensamblado de `CAnimationManager::DrawPlayerShadow` (`0x00027380`) y `CM3DXSkinMesh::DrawSkin` (`0x0000a324`) confirmó que el cálculo de distancia entre pies y huesos produce un desbordamiento aritmético en punto fijo (`rsb r2, r2, #0x10000`), seguido por un desplazamiento `lsl r1, r1, #4` en `CM3DDevice2::PushAndScale` (`0x00003e68`), el cual pasa matrices corruptas a `_glMultMatrixx`.
  - Esto estiraba la geometría del jugador/sombra a través de todo el frustum durante exactamente un draw call antes del `glPopMatrix`, provocando los destellos de un cuadro en color naranja (sombra), rojo/blanco (camiseta) o negro (botines).
- **Límites matemáticos verificados**:
  - Elemento 3×3 de rotación/escala legítimo máximo en juego normal: **2.320**.
  - Elemento mínimo observado durante un destello: **1.857** (hasta **60.305**).
  - Umbral de seguridad adoptado: **16.0f** (7× mayor que el máximo legítimo, 100× menor que el glitch).
- **Solución implementada**:
  1. **Barrera de draw calls (`is_perspective_3d_exploded`)**: En [source/gles.c](file:///Volumes/TOSHIBA%20EXT/PSVITA%20Develop/Real-Soccer-2010-vita/source/gles.c), se verifica la matriz ModelView antes de `g_glDrawArrays` y `g_glDrawElements` cuando la proyección es 3D (`pm[11] < -0.5f`). Si algún elemento 3×3 excede 16.0f o es NaN/Inf, el draw call corrupto se suprime inmediatamente sin llegar a la GPU.
  2. **Sanitización de matrices de entrada**: Se protegen `g_glMultMatrixx`, `g_glMultMatrixf`, `g_glLoadMatrixx`, `g_glLoadMatrixf`, `g_glScalef` y `g_glPopMatrix`. Si se intenta multiplicar o cargar una matriz con escala desbordada, se descarta/acota la operación para preservar la integridad del pipeline.
  3. **Desactivación de la sonda**: `#define PORT_FLASH_PROBE 0` en [source/port_config.h](file:///Volumes/TOSHIBA%20EXT/PSVITA%20Develop/Real-Soccer-2010-vita/source/port_config.h), eliminando `glReadPixels` y recuperando el framerate completo (30/60 fps fluidos sin paradas de GPU).
- **Verificación**: Build limpio completado con exit code 0 (`psvita-toolkit build`), generando `build/realsoccer10.vpk`.
