#include "i18n.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "menus/core/ui_element.h"
#include "menus/core/ui_screen.h"

int language = LANG_ENGLISH;

typedef struct {
    const char *en;
    const char *es;
    const char *it;
    const char *pt;
} Translation;

// The fonts are ASCII only: no accents, no enye. All four languages are
// written like that (Portuguese: "configuracoes", "nao", ...).
static const Translation translations[] = {
    // ---- general UI -------------------------------------------------------
    { "Settings", "Ajustes", "Impostazioni", "Configuracoes" },
    { "Options", "Opciones", "Opzioni", "Opcoes" },
    { "Guide", "Guia", "Guida", "Guia" },
    { "Credits", "Creditos", "Crediti", "Creditos" },
    { "Loading...", "Cargando...", "Caricamento...", "Carregando..." },
    { "Songs", "Canciones", "Canzoni", "Cancoes" },
    { "Soundtrack", "Banda sonora", "Colonna sonora", "Trilha sonora" },
    { "Music", "Musica", "Musica", "Musica" },
    { "Sounds", "Sonidos", "Suoni", "Sons" },
    { "SFX", "Efectos", "Effetti", "Efeitos" },
    { "Glow", "Brillo", "Bagliore", "Brilho" },
    { "Graphics", "Graficos", "Grafica", "Graficos" },
    { "Input", "Controles", "Controlli", "Controles" },
    { "Misc", "Varios", "Vari", "Varios" },
    { "Gameplay", "Juego", "Gioco", "Jogo" },
    { "Cosmetic", "Estetica", "Estetica", "Estetica" },
    { "Language", "Idioma", "Lingua", "Idioma" },
    { "Stats", "Estadisticas", "Statistiche", "Estatisticas" },
    { "Level Stats", "Estadisticas del nivel", "Statistiche livello", "Estatisticas do nivel" },
    { "Level Info", "Info del nivel", "Info livello", "Info do nivel" },
    { "Level Name", "Nombre del nivel", "Nome livello", "Nome do nivel" },
    { "Info", "Info", "Info", "Info" },
    { "Notice", "Aviso", "Avviso", "Aviso" },
    { "Profile", "Perfil", "Profilo", "Perfil" },
    { "Player name", "Nombre del jugador", "Nome giocatore", "Nome do jogador" },
    { "Triangle: change name", "Triangulo: cambiar nombre", "Triangolo: cambia nome", "Triangulo: mudar nome" },
    { "D-pad: choose   Cross: type   R: A/a   START: OK   B: cancel",
      "Cruceta: elegir   Cross: escribir   R: A/a   START: OK   B: cancelar",
      "Croce: scegli   Cross: scrivi   R: A/a   START: OK   B: annulla",
      "Direcional: escolher   Cross: digitar   R: A/a   START: OK   B: cancelar" },
    { "SPACE", "ESPACIO", "SPAZIO", "ESPACO" },
    { "DEL", "BORRAR", "CANCELLA", "APAGAR" },
    { "CANCEL", "CANCELAR", "ANNULLA", "CANCELAR" },
    { "OK", "OK", "OK", "OK" },
    { "Yes", "Si", "Si", "Sim" },
    { "No", "No", "No", "Nao" },
    { "Cancel", "Cancelar", "Annulla", "Cancelar" },
    { "Next", "Siguiente", "Avanti", "Proximo" },
    { "Exit", "Salir", "Esci", "Sair" },
    { "View", "Ver", "Vedi", "Ver" },
    { "Back", "Volver", "Indietro", "Voltar" },
    { "Quit Game", "Salir del juego", "Esci dal gioco", "Sair do jogo" },
    { "Delete Level", "Borrar nivel", "Elimina livello", "Apagar nivel" },
    { "Clear Filters", "Borrar filtros", "Cancella filtri", "Limpar filtros" },
    { "Advanced Options", "Opciones avanzadas", "Opzioni avanzate", "Opcoes avancadas" },
    { "I understand", "Entiendo", "Ho capito", "Entendi" },
    { "Coming Soon!", "Proximamente!", "Prossimamente!", "Em breve!" },
    { "N/A", "N/D", "N/D", "N/D" },
    { "Unknown", "Desconocido", "Sconosciuto", "Desconhecido" },
    { "By Unknown", "Por desconocido", "Di sconosciuto", "Por desconhecido" },
    { "By", "Por", "Di", "Por" },
    { "By: Song Artist", "Por: artista", "Di: artista", "Por: artista" },
    { "No description found.", "Sin descripcion.", "Nessuna descrizione.", "Sem descricao." },
    { "No levels found!", "No se encontraron niveles!", "Nessun livello trovato!", "Nenhum nivel encontrado!" },
    { "Map packs", "Map packs", "Map packs", "Map packs" },
    { "No map packs found!", "No se encontraron map packs!", "Nessun map pack trovato!", "Nenhum map pack encontrado!" },
    { "Download them from the project's GitHub and<p>copy the packs to mstick\\psp\\gdpsp\\mappacks\\",
      "Descargalos desde el GitHub del proyecto y<p>copialos en mstick\\psp\\gdpsp\\mappacks\\",
      "Scaricali dal GitHub del progetto e<p>copiali in mstick\\psp\\gdpsp\\mappacks\\",
      "Baixe-os do GitHub do projeto e<p>copie-os em mstick\\psp\\gdpsp\\mappacks\\" },
    { "Level is up to date.", "El nivel esta actualizado.", "Il livello e aggiornato.", "O nivel esta atualizado." },
    { "Update", "Actualizar", "Aggiorna", "Atualizar" },
    { "Downloads", "Descargas", "Download", "Downloads" },
    { "Likes", "Me gusta", "Mi piace", "Curtidas" },
    { "Magic", "Magia", "Magia", "Magia" },
    { "Featured", "Destacados", "In evidenza", "Destaques" },
    { "Recent", "Recientes", "Recenti", "Recentes" },
    { "Trending", "Tendencia", "Di tendenza", "Em alta" },
    { "Quick Search", "Busqueda rapida", "Ricerca rapida", "Busca rapida" },
    { "Saved Levels", "Niveles guardados", "Livelli salvati", "Niveis salvos" },
    { "Browse your saved user levels!", "Mira tus niveles guardados!", "Guarda i tuoi livelli salvati!", "Veja seus niveis salvos!" },
    { "Play your imported .gmd files!", "Juga tus archivos .gmd!", "Gioca i tuoi file .gmd importati!", "Jogue seus arquivos .gmd importados!" },
    { "Search for user-generated<p>levels online!",
      "Busca niveles creados<p>por usuarios en linea!",
      "Cerca livelli<p>creati dagli utenti online!",
      "Busque niveis<p>criados por usuarios online!" },
    { "Comments", "Comentarios", "Commenti", "Comentarios" },
    { "Length Filter", "Filtro de duracion", "Filtro durata", "Filtro de duracao" },
    { "Song Filter", "Filtro de cancion", "Filtro canzone", "Filtro de cancao" },
    { "Song", "Cancion", "Canzone", "Cancao" },
    { "Length", "Duracion", "Durata", "Duracao" },
    { "Enable", "Activar", "Attiva", "Ativar" },
    { "Short", "Corta", "Corta", "Curta" },
    { "Medium", "Media", "Media", "Media" },
    { "Long", "Larga", "Lunga", "Longa" },
    { "Tiny", "Diminuta", "Minuscola", "Minusculo" },
    { "Completed", "Completados", "Completati", "Completados" },
    { "Uncompleted", "No completados", "Non completati", "Nao completados" },
    { "Original", "Original", "Originale", "Original" },
    { "No Star", "Sin estrella", "Senza stella", "Sem estrela" },
    { "Star", "Estrella", "Stella", "Estrela" },
    { "Custom", "Personalizada", "Personalizzata", "Personalizada" },
    { "Normal", "Normal", "Normale", "Normal" },
    { "Using song: N/A", "Cancion: N/D", "Canzone: N/D", "Cancao: N/D" },
    { "Using song:", "Cancion:", "Canzone:", "Cancao:" },
    { "(NOT FOUND)", "(NO ENCONTRADO)", "(NON TROVATO)", "(NAO ENCONTRADO)" },

    // ---- game modes -------------------------------------------------------
    { "Cube", "Cubo", "Cubo", "Cubo" },
    { "Ship", "Nave", "Nave", "Nave" },
    { "Ball", "Bola", "Sfera", "Bola" },
    { "Ufo", "UFO", "UFO", "UFO" },
    { "Wave", "Wave", "Wave", "Wave" },
    { "Orbs", "Orbes", "Orbe", "Orbes" },

    // ---- main menu / disclaimers -----------------------------------------
    { "This port is a WIP!", "Este port esta en desarrollo!", "Questa conversione e in sviluppo!", "Este port esta em desenvolvimento!" },
    { "Many things don't work yet and you might",
      "Muchas cosas no andan todavia y podes",
      "Molte cose non funzionano e potresti",
      "Muitas coisas nao funcionam e voce pode" },
    { "find game-breaking bugs in various places.",
      "encontrar errores graves en varios lugares.",
      "trovare bug gravi in vari punti.",
      "encontrar bugs graves em varios lugares." },
    { "To report issues and download the latest",
      "Para reportar errores y bajar la ultima",
      "Per segnalare problemi e scaricare l'ultima",
      "Para reportar problemas e baixar a ultima" },
    { "version, please visit the project's GitHub.",
      "version, visita el GitHub del proyecto.",
      "versione, visita il GitHub del progetto.",
      "versao, visite o GitHub do projeto." },
    { "This feature has NOT", "Esta funcion NO", "Questa funzione NON", "Este recurso NAO" },
    { "been implemented yet!", "esta implementada!", "e implementata!", "foi implementado!" },
    { "Hang tight! It might be added<p> in the future.",
      "Espera! Puede agregarse<p> en el futuro.",
      "Aspetta! Potrebbe essere aggiunta<p> in futuro.",
      "Aguarde! Pode ser adicionado<p> no futuro." },
    { "This game must not be sold", "Este juego no debe venderse", "Questo gioco non deve essere venduto", "Este jogo nao deve ser vendido" },
    { "in any way, shape or form.", "de ninguna forma.", "in alcun modo.", "de nenhuma forma." },
    { "If you paid for this, ask for a", "Si pagaste por esto, pedi un", "Se hai pagato per questo, chiedi un", "Se voce pagou por isto, peca um" },
    { "refund immediately!", "reembolso ya!", "rimborso immediato!", "reembolso imediatamente!" },
    { "Geometry Dash and its respective", "Geometry Dash y sus respectivos", "Geometry Dash e i suoi rispettivi", "Geometry Dash e seus respectivos" },
    { "resources are by RobTop Games.", "recursos son de RobTop Games.", "contenuti sono di RobTop Games.", "recursos sao da RobTop Games." },
    { "Welcome to robert game", "Bienvenido a robert game", "Benvenuto in robert game", "Bem-vindo ao robert game" },

    // ---- confirmations / filters -----------------------------------------
    { "Are you sure you want to <#ff0000>delete</> this<p>level?",
      "Seguro que queres <#ff0000>borrar</> este<p>nivel?",
      "Sei sicuro di voler <#ff0000>eliminare</> questo<p>livello?",
      "Tem certeza que quer <#ff0000>apagar</> este<p>nivel?" },
    { "Are you sure you want to quit?", "Seguro que queres salir?", "Sei sicuro di voler uscire?", "Tem certeza que quer sair?" },
    { "Do you want to <#ff0000>reset</> all<p>search filters?",
      "Queres <#ff0000>reiniciar</> todos<p>los filtros?",
      "Vuoi <#ff0000>reimpostare</> tutti<p>i filtri?",
      "Quer <#ff0000>reiniciar</> todos<p>os filtros?" },
    { "For more detailed instructions,<p>check the project's GitHub.",
      "Para mas instrucciones,<p>mira el GitHub del proyecto.",
      "Per istruzioni piu dettagliate,<p>vedi il GitHub del progetto.",
      "Para instrucoes mais detalhadas,<p>veja o GitHub do projeto." },
    { "Game Version:", "Version del juego:", "Versione gioco:", "Versao do jogo:" },
    { "Stars Requested:", "Estrellas pedidas:", "Stelle richieste:", "Estrelas pedidas:" },
    { "Updated:", "Actualizado:", "Aggiornato:", "Atualizado:" },
    { "Uploaded:", "Subido:", "Caricato:", "Enviado:" },
    { "<#ffff00>By Level Author</>", "<#ffff00>Por el autor</>", "<#ffff00>Dell'autore</>", "<#ffff00>Pelo autor</>" },
    { "<#3F2215>ID: N/A</>", "<#3F2215>ID: N/D</>", "<#3F2215>ID: N/D</>", "<#3F2215>ID: N/D</>" },
    { "<#78aaf0>ID: N/A</>", "<#78aaf0>ID: N/D</>", "<#78aaf0>ID: N/D</>", "<#78aaf0>ID: N/D</>" },

    // ---- gameplay / pause / level complete -------------------------------
    { "Levle", "Nivel", "Livello", "Nivel" },
    { "Attempts:", "Intentos:", "Tentativi:", "Tentativas:" },
    { "Jumps:", "Saltos:", "Salti:", "Saltos:" },
    { "Time:", "Tiempo:", "Tempo:", "Tempo:" },
    { "Normal mode", "Modo normal", "Modalita normale", "Modo normal" },
    { "Practice Mode", "Modo practica", "Modalita pratica", "Modo pratica" },
    { "Practice mode", "Modo practica", "Modalita pratica", "Modo pratica" },
    { "<#40e348>Total Attempts</>:", "<#40e348>Intentos totales</>:", "<#40e348>Tentativi totali</>:", "<#40e348>Tentativas totais</>:" },
    { "<#60abef>Total Jumps</>:", "<#60abef>Saltos totales</>:", "<#60abef>Salti totali</>:", "<#60abef>Saltos totais</>:" },
    { "<#ff00ff>Normal</>:", "<#ff00ff>Normal</>:", "<#ff00ff>Normale</>:", "<#ff00ff>Normal</>:" },
    { "<#ffa54b>Practice</>:", "<#ffa54b>Practica</>:", "<#ffa54b>Pratica</>:", "<#ffa54b>Pratica</>:" },
    { "Safe mode", "Modo seguro", "Modalita sicura", "Modo seguro" },
    { "Noclip Accuracy:", "Precision noclip:", "Precisione noclip:", "Precisao noclip:" },
    { "Well done... Now try to complete it<p>without any checkpoints!",
      "Bien hecho... Ahora completalo<p>sin ningun checkpoint!",
      "Ben fatto... Ora provalo<p>senza checkpoint!",
      "Muito bem... Agora tente completar<p>sem nenhum checkpoint!" },
    { "Impressive", "Impresionante", "Impressionante", "Impressionante" },
    { "Awesome!", "Genial!", "Fantastico!", "Fantastico!" },
    { "Not bad!", "No esta mal!", "Niente male!", "Nada mal!" },
    { "Well done!", "Bien hecho!", "Ben fatto!", "Muito bem!" },
    { "Challenge Breaker!", "Rompe desafios!", "Spezza sfide!", "Quebra desafios!" },
    { "Warp Speed!", "Velocidad warp!", "Velocita warp!", "Velocidade warp!" },
    { "You are... The One!", "Vos sos... El Elegido!", "Tu sei... L'Eletto!", "Voce e... O Escolhido!" },
    { "How is this possible!?", "Como es posible!?", "Come e possibile!?", "Como isso e possivel!?" },
    { "You beat me...", "Me ganaste...", "Mi hai battuto...", "Voce me venceu..." },
    { "Reflex Master!", "Maestro del reflejo!", "Maestro di riflessi!", "Mestre dos reflexos!" },
    { "Skillful!", "Habilidoso!", "Abile!", "Habilidoso!" },
    { "Y u do dis?", "Por que haces esto?", "Perche fai questo?", "Por que faz isso?" },
    { "Good Job!", "Buen trabajo!", "Bel lavoro!", "Bom trabalho!" },
    { "Incredible!", "Increible!", "Incredibile!", "Incrivel!" },
    { "I am speechless...", "No tengo palabras...", "Sono senza parole...", "Estou sem palavras..." },
    { "Brilliant!", "Brillante!", "Geniale!", "Brilhante!" },
    { "Not 1 attempt", "No en 1 intento", "Non al primo tentativo", "Nao em 1 tentativa" },
    { "You beat this instead of Story Madness...",
      "Ganaste esto en vez de Story Madness...",
      "Hai battuto questo invece di Story Madness...",
      "Voce venceu isto em vez de Story Madness..." },
    { "That was kinda sloppy", "Eso fue medio flojo", "E stato un po' sciatto", "Foi meio desleixado" },
    { "Well done... now beat it in the PC version",
      "Bien... ahora ganalo en la version de PC",
      "Ben fatto... ora battilo nella versione PC",
      "Muito bem... agora venca na versao de PC" },
    { "Good, now beat it with your eyes closed",
      "Bien, ahora ganalo con los ojos cerrados",
      "Bene, ora battilo a occhi chiusi",
      "Bom, agora venca de olhos fechados" },
    { "CBF detected, loser!", "CBF detectado, perdedor!", "CBF rilevato, perdente!", "CBF detectado, perdedor!" },
    { "Hacked. This level is clearly impossible",
      "Hackeado. Este nivel es claramente imposible",
      "Hackerato. Questo livello e chiaramente impossibile",
      "Hackeado. Este nivel e claramente impossivel" },
    { "Noclip Accuracy: 0.01%", "Precision noclip: 0.01%", "Precisione noclip: 0.01%", "Precisao noclip: 0.01%" },
    { "Would be better if it was a harder level",
      "Estaria mejor si fuera un nivel mas dificil",
      "Sarebbe meglio se fosse piu difficile",
      "Seria melhor se fosse um nivel mais dificil" },
    { "Would be better if it was an easier level",
      "Estaria mejor si fuera un nivel mas facil",
      "Sarebbe meglio se fosse piu facile",
      "Seria melhor se fosse um nivel mais facil" },
    { "Auto Safe Mode cheat detected: Using a PSP",
      "Trampa de modo seguro: Usando una PSP",
      "Truffa modalita sicura: usando una PSP",
      "Trapaca de modo seguro: usando um PSP" },
    { "Auto Safe Mode cheat detected: Using a 3DS",
      "Trampa de modo seguro: Usando una 3DS",
      "Truffa modalita sicura: usando un 3DS",
      "Trapaca de modo seguro: usando um 3DS" },
    { "Auto Safe Mode cheat detected:<p>Noclipped through the end wall",
      "Trampa de modo seguro:<p>Pase la pared final en noclip",
      "Truffa modalita sicura:<p>Passato in noclip dal muro finale",
      "Trapaca de modo seguro:<p>Passei do muro final em noclip" },
    { "I lied, you got 99%", "Te menti, sacaste 99%", "Ho mentito, hai fatto 99%", "Eu menti, voce fez 99%" },
    { "I have no words.... oh wait", "No tengo palabras.... ah, espera", "Non ho parole.... ah, aspetta", "Nao tenho palavras.... ah, espera" },
    { "we really doing anything now", "que estamos haciendo ahora", "cosa stiamo facendo adesso", "o que estamos fazendo agora" },

    // ---- how to play ------------------------------------------------------
    { "<#00FF00>Hold</> to fly up.<p><p><#00FF00>Release</> to fly down.",
      "<#00FF00>Manten</> para volar arriba.<p><p><#00FF00>Solta</> para bajar.",
      "<#00FF00>Tieni</> per volare su.<p><p><#00FF00>Rilascia</> per scendere.",
      "<#00FF00>Segure</> para voar para cima.<p><p><#00FF00>Solte</> para descer." },
    { "<#00FF00>Hold</> to move upwards.<p><p><#00FF00>Release</> to go back<p>down.",
      "<#00FF00>Manten</> para subir.<p><p><#00FF00>Solta</> para volver<p>abajo.",
      "<#00FF00>Tieni</> per salire.<p><p><#00FF00>Rilascia</> per tornare<p>giu.",
      "<#00FF00>Segure</> para subir.<p><p><#00FF00>Solte</> para voltar<p>para baixo." },
    { "<#00FF00>Tap</> the screen or<p><#00FF00>press</> A to jump.<p><p><#00FF00>Hold</> down to keep<p>jumping.",
      "<#00FF00>Toca</> la pantalla o<p><#00FF00>apreta</> A para saltar.<p><p><#00FF00>Manten</> para seguir<p>saltando.",
      "<#00FF00>Tocca</> lo schermo o<p><#00FF00>premi</> A per saltare.<p><p><#00FF00>Tieni</> premuto per<p>continuare a saltare.",
      "<#00FF00>Toque</> a tela ou<p><#00FF00>aperte</> A para pular.<p><p><#00FF00>Segure</> para continuar<p>pulando." },
    { "<#00FF00>Tap</> the screen or<p><#00FF00>press</> X to jump.<p><p><#00FF00>Hold</> down to keep<p>jumping.",
      "<#00FF00>Toca</> la pantalla o<p><#00FF00>apreta</> X para saltar.<p><p><#00FF00>Manten</> para seguir<p>saltando.",
      "<#00FF00>Tocca</> lo schermo o<p><#00FF00>premi</> X per saltare.<p><p><#00FF00>Tieni</> premuto per<p>continuare a saltare.",
      "<#00FF00>Toque</> a tela ou<p><#00FF00>aperte</> X para pular.<p><p><#00FF00>Segure</> para continuar<p>pulando." },
    { "<#00FF00>Tap</> to boost upwards.", "<#00FF00>Toca</> para impulsarte.", "<#00FF00>Tocca</> per spingerti su.", "<#00FF00>Toque</> para impulsionar para cima." },
    { "<#00FF00>Tap</> to switch gravity.", "<#00FF00>Toca</> para cambiar gravedad.", "<#00FF00>Tocca</> per cambiare gravita.", "<#00FF00>Toque</> para mudar a gravidade." },
    { "<#599cff>Practice mode</> lets<p>you place <#00FF00>checkpoints</>.<p><p>Enter it from the<p>pause menu.",
      "<#599cff>El modo practica</> te deja<p>poner <#00FF00>checkpoints</>.<p><p>Entra desde el<p>menu de pausa.",
      "<#599cff>La modalita pratica</> ti fa<p>mettere <#00FF00>checkpoint</>.<p><p>Entra dal<p>menu pausa.",
      "<#599cff>O modo pratica</> deixa<p>colocar <#00FF00>checkpoints</>.<p><p>Entre pelo<p>menu de pausa." },
    { "Tap while touching <p>a <#FFFF00>Jump Orb</> to use<p>its effect.",
      "Toca un <#FFFF00>orbe</> mientras<p>lo tocas para usar<p>su efecto.",
      "Tocca un <#FFFF00>orbe</> mentre<p>lo tocchi per usare<p>il suo effetto.",
      "Toque em um <#FFFF00>orbe</> enquanto<p>o toca para usar<p>o efeito." },
    { "You can place them <p>manually, or use <p>the <#00FF00>auto-checkpoints</><p>feature.",
      "Los podes poner <p>a mano, o usar <p>los <#00FF00>checkpoints auto</><p>.",
      "Puoi metterli <p>a mano, o usare <p>i <#00FF00>checkpoint auto</><p>.",
      "Voce pode colocar <p>manualmente, ou usar <p>os <#00FF00>checkpoints auto</><p>." },

    // ---- import instructions (paths stay as they are) --------------------
    { "To import levels, go to mstick\\psp\\gdpsp\\ next to<p>the EBOOT on your Memory Stick and copy the .gmd's<p>of your choice along with their matching songs to<p>\\external_levels\\ and \\saved_songs\\ respectively.",
      "Para importar niveles, anda a mstick\\psp\\gdpsp\\ junto<p>al EBOOT en tu Memory Stick y copia los .gmd<p>que quieras con sus canciones en<p>\\external_levels\\ y \\saved_songs\\.",
      "Per importare livelli, vai in mstick\\psp\\gdpsp\\ accanto<p>all'EBOOT sulla Memory Stick e copia i .gmd<p>che vuoi con le loro canzoni in<p>\\external_levels\\ e \\saved_songs\\.",
      "Para importar niveis, va em mstick\\psp\\gdpsp\\ ao lado<p>do EBOOT no Memory Stick e copie os .gmd<p>que quiser com suas cancoes em<p>\\external_levels\\ e \\saved_songs\\." },
    { "To import levels, navigate to \\3ds\\gd3ds\\<p>on your SD card and copy the .gmd's<p>of your choice along with their matching<p>songs to \\external_levels\\ and<p>\\custom_songs\\ respectively.",
      "Para importar niveles, anda a \\3ds\\gd3ds\\<p>en tu tarjeta SD y copia los .gmd<p>que quieras con sus canciones en<p>\\external_levels\\ y<p>\\custom_songs\\.",
      "Per importare livelli, vai in \\3ds\\gd3ds\\<p>sulla scheda SD e copia i .gmd<p>che vuoi con le loro canzoni in<p>\\external_levels\\ e<p>\\custom_songs\\.",
      "Para importar niveis, va em \\3ds\\gd3ds\\<p>no cartao SD e copie os .gmd<p>que quiser com suas cancoes em<p>\\external_levels\\ e<p>\\custom_songs\\." },

    // ---- statistics -------------------------------------------------------
    { "Total Jumps", "Saltos totales", "Salti totali", "Saltos totais" },
    { "Total Attempts", "Intentos totales", "Tentativi totali", "Tentativas totais" },
    { "Collected Stars", "Estrellas totales", "Stelle totali", "Estrelas totais" },
    { "Completed Levels", "Niveles completados", "Livelli completi", "Niveis completos" },
    { "Completed Ext. Levels", "Niveles externos", "Livelli esterni", "Niveis externos" },
    { "Completed Demon Levels", "Niveles con Demons", "Livelli con Demons", "Niveis com Demons" },
    { "Collected Secret Coins", "Monedas secretas", "Monete segrete", "Moedas secretas" },
    { "Collected User Coins", "Monedas de usuario", "Monete utente", "Moedas de usuario" },
    { "Players Destroyed", "Destruidos", "Distrutti", "Destruidos" },
    { "Stars", "Estrellas", "Stelle", "Estrelas" },
    { "Secret Coins", "Monedas", "Monete", "Moedas" },
    { "Levels", "Niveles", "Livelli", "Niveis" },
    { "Attempts", "Intentos", "Tentativi", "Tentativas" },
    { "Jumps", "Saltos", "Salti", "Saltos" },

    // ---- credits ----------------------------------------------------------
    { "Lead Developer", "Desarrollador principal", "Sviluppatore principale", "Desenvolvedor principal" },
    { "Pathfinder (Physics)", "Fisica", "Fisica", "Fisica" },
    { "Menus and VFX", "Menus y efectos", "Menu ed effetti", "Menus e efeitos" },
    { "VFX", "VFX", "VFX", "VFX" },
    { "Concepts, Menus", "Conceptos, menus", "Concetti, menu", "Conceitos, menus" },
    { "Menus", "Menus", "Menu", "Menus" },
    { "Optimization", "Optimizacion", "Ottimizzazione", "Otimizacao" },
    { "240hz input", "Input 240hz", "Input 240hz", "Input 240hz" },
    { "3D Support", "Soporte 3D", "Supporto 3D", "Suporte 3D" },
    { "PSP Port", "Port a PSP", "Port per PSP", "Port para PSP" },
    { "PSP Port Helper", "Ayudante del port", "Aiuto al port", "Ajudante do port" },
    { "PSP Testing", "Testeo en PSP", "Test su PSP", "Testes no PSP" },
    { "Thanks", "Gracias", "Grazie", "Obrigado" },
    { "to everyone who tested on a real PSP",
      "a todos los que testearon en PSP real",
      "a tutti quelli che hanno testato su PSP reale",
      "a todos que testaram em um PSP real" },

    // ---- options ----------------------------------------------------------
    { "800 px res", "Res 800 px", "Res 800 px", "Res 800 px" },
    { "Stereoscopic 3D", "3D estereoscopico", "3D stereoscopico", "3D estereoscopico" },
    { "60 FPS Switch", "Cambio a 60 FPS", "Interruttore 60 FPS", "Chave 60 FPS" },
    { "Show FPS", "Mostrar FPS", "Mostra FPS", "Mostrar FPS" },
    { "Performance mode", "Modo rendimiento", "Modalita prestazioni", "Modo desempenho" },
    { "Disables more graphical features<p>in order to prioritize performance.",
      "Deshabilita mas funciones graficas<p>con tal de priorizar el rendimiento.",
      "Disattiva altre funzioni grafiche<p>per dare priorita alle prestazioni.",
      "Desativa mais funcoes graficas<p>para priorizar o desempenho." },
    { "Disable particles", "Sin particulas", "Niente particelle", "Sem particulas" },
    { "Enable object glow", "Brillo de objetos", "Bagliore oggetti", "Brilho dos objetos" },
    { "Square to jump", "Cuadrado para saltar", "Quadrato per saltare", "Quadrado para pular" },
    { "Y to jump", "Y para saltar", "Y per saltare", "Y para pular" },
    { "Global tap effect", "Efecto de toque global", "Effetto tocco globale", "Efeito de toque global" },
    { "Enable debug keys", "Teclas de debug", "Tasti debug", "Teclas de debug" },
    { "Write log", "Escribir log", "Scrivi log", "Escrever log" },
    { "Writes psp_log.txt to the Memory Stick.<p>Disable to reduce I/O stutter.",
      "Escribe psp_log.txt en la Memory Stick.<p>Desactiva para reducir tirones de I/O.",
      "Scrive psp_log.txt sulla Memory Stick.<p>Disattiva per ridurre i scatti I/O.",
      "Escreve psp_log.txt na Memory Stick.<p>Desativa para reduzir travamentos de I/O." },
    { "240Hz input (CBF)", "Input 240Hz (CBF)", "Input 240Hz (CBF)", "Input 240Hz (CBF)" },
    { "Show hitboxes", "Mostrar hitboxes", "Mostra hitbox", "Mostrar hitboxes" },
    { "Enable hitbox trail", "Estela de hitboxes", "Scia hitbox", "Rastro de hitboxes" },
    { "Show hitboxes<p>on death", "Hitboxes<p>al morir", "Hitbox<p>alla morte", "Hitboxes<p>ao morrer" },
    { "Do not", "No lo hagas...", "Non fare", "Nao fazer" },
    { "Practice music sync", "Musica en practica", "Musica in pratica", "Musica na pratica" },
    { "Show progress bar", "Barra de progreso", "Barra di progresso", "Barra de progresso" },
    { "Show level percentage", "Porcentaje del nivel", "Percentuale livello", "Porcentagem do nivel" },
    { "Accurate percentage", "Porcentaje exacto", "Percentuale precisa", "Porcentagem exata" },
    { "Ultra percentage", "Ultra porcentaje", "Ultra percentuale", "Ultra porcentagem" },
    { "Quick retry", "Reintento rapido", "Riprova rapida", "Reinicio rapido" },
    { "Auto checkpoints", "Checkpoints auto", "Checkpoint auto", "Checkpoints auto" },
    { "Quick checkpoints", "Checkpoints rapidos", "Checkpoint rapidi", "Checkpoints rapidos" },
    { "Switch trail color", "Cambiar color de estela", "Cambia colore scia", "Mudar cor do rastro" },
    { "Switch wave<p>trail color", "Cambiar color<p>de estela wave", "Cambia colore<p>scia wave", "Mudar cor<p>do rastro wave" },
    { "Solid wave trail", "Estela wave solida", "Scia wave piena", "Rastro wave solido" },
    { "Disable player trail", "Sin estela del jugador", "Niente scia giocatore", "Sem rastro do jogador" },
    { "No wave trail behind", "Sin estela en wave", "Niente scia dietro", "Sem rastro atras" },
    { "Noclip accuracy", "Precision noclip", "Precisione noclip", "Precisao noclip" },

    { "Doubles the top screen's horizontal<p>resolution.",
      "Duplica la resolucion horizontal<p>de la pantalla superior.",
      "Raddoppia la risoluzione orizzontale<p>dello schermo superiore.",
      "Dobra a resolucao horizontal<p>da tela superior." },
    { "Adds depth to the top screen.<p>Use the 3D slider, costs some FPS.",
      "Agrega profundidad a la pantalla.<p>Usa el slider 3D, cuesta FPS.",
      "Aggiunge profondita allo schermo.<p>Usa il cursore 3D, costa FPS.",
      "Adiciona profundidade a tela.<p>Use o slider 3D, custa FPS." },
    { "Runs at 60 FPS instead of 30.<p>Heavy levels may not keep up<p>on a PSP-1000.",
      "Corre a 60 FPS en vez de 30.<p>Los niveles pesados pueden<p>no llegar en una PSP-1000.",
      "Va a 60 FPS invece di 30.<p>I livelli pesanti potrebbero<p>non reggere su una PSP-1000.",
      "Roda a 60 FPS em vez de 30.<p>Niveis pesados podem nao<p>acompanhar em um PSP-1000." },
    { "Shows the frame rate in the<p>top left corner.",
      "Muestra los FPS arriba<p>a la izquierda.",
      "Mostra gli FPS in<p>alto a sinistra.",
      "Mostra os FPS no<p>canto superior esquerdo." },
    { "Swaps your jump input to Square.", "Cambia el salto a Cuadrado.", "Cambia il salto su Quadrato.", "Troca o pulo para Quadrado." },
    { "Swaps your jump input to Y.", "Cambia el salto a Y.", "Cambia il salto su Y.", "Troca o pulo para Y." },
    { "Plays the tap effect across all menus.",
      "Reproduce el efecto de toque en todos los menus.",
      "Riproduce l'effetto tocco in tutti i menu.",
      "Reproduz o efeito de toque em todos os menus." },
    { "Enables debug key shortcuts.<p>(Circle + L, Circle + R, Triangle)",
      "Activa atajos de debug.<p>(Circulo + L, Circulo + R, Triangulo)",
      "Attiva scorciatoie debug.<p>(Cerchio + L, Cerchio + R, Triangolo)",
      "Ativa atalhos de debug.<p>(Circulo + L, Circulo + R, Triangulo)" },
    { "Enables debug key shortcuts.<p>(B + L, B + R, X)",
      "Activa atajos de debug.<p>(B + L, B + R, X)",
      "Attiva scorciatoie debug.<p>(B + L, B + R, X)",
      "Ativa atalhos de debug.<p>(B + L, B + R, X)" },
    { "Registers jumps on the exact 240Hz<p>physics tick they were pressed on.<p>Precision is reduced below 30fps.",
      "Registra saltos en el tick de fisica<p>de 240Hz exacto en que se apretaron.<p>La precision baja debajo de 30fps.",
      "Registra i salti sul tick fisico<p>esatto a 240Hz in cui sono premuti.<p>La precisione cala sotto i 30fps.",
      "Registra pulos no tick de fisica<p>exato de 240Hz em que foram apertados.<p>A precisao cai abaixo de 30fps." },
    { "Shows object hitboxes while in a level.<p>WARNING: AFFECTS PERFOMANCE!",
      "Muestra hitboxes en el nivel.<p>OJO: AFECTA EL RENDIMIENTO!",
      "Mostra le hitbox nel livello.<p>ATTENZIONE: RIDUCE LE PRESTAZIONI!",
      "Mostra hitboxes no nivel.<p>ATENCAO: AFETA O DESEMPENHO!" },
    { "Doesn't do anything...<p>Well, nothing useful.",
      "No hace nada...<p>Bueno, nada util.",
      "Non fa niente...<p>Beh, niente di utile.",
      "Nao faz nada...<p>Bom, nada util." },
    { "Plays the level's music in practice mode.",
      "Reproduce la musica del nivel en practica.",
      "Riproduce la musica del livello in pratica.",
      "Reproduz a musica do nivel na pratica." },
    { "Shows level progress with 2 decimals.",
      "Muestra el progreso con 2 decimales.",
      "Mostra il progresso con 2 decimali.",
      "Mostra o progresso com 2 decimais." },
    { "But mom, I want more decimals!!!!<p>(Why would you want to use this?)",
      "Pero mama, quiero mas decimales!!!!<p>(Por que usarias esto?)",
      "Ma mamma, voglio piu decimali!!!!<p>(Perche useresti questo?)",
      "Mas mae, quero mais decimais!!!!<p>(Por que usaria isto?)" },
    { "Restarts in 0.5 seconds instead of 1.",
      "Reinicia en 0.5 segundos en vez de 1.",
      "Riparte in 0.5 secondi invece di 1.",
      "Reinicia em 0.5 segundos em vez de 1." },
    { "Makes auto checkpoints appear quicker.",
      "Hace que los checkpoints auto aparezcan antes.",
      "Fa apparire prima i checkpoint auto.",
      "Faz os checkpoints auto aparecerem antes." },
    { "Makes the player trail use P1<p>instead of P2.",
      "Hace que la estela use P1<p>en vez de P2.",
      "Fa usare P1 alla scia<p>invece di P2.",
      "Faz o rastro usar P1<p>em vez de P2." },
    { "Makes the wave trail use P1<p>instead of P2.",
      "Hace que la estela wave use P1<p>en vez de P2.",
      "Fa usare P1 alla scia wave<p>invece di P2.",
      "Faz o rastro wave usar P1<p>em vez de P2." },
    { "Disables blending for the wave trail.",
      "Desactiva el blending de la estela wave.",
      "Disattiva il blending per la scia wave.",
      "Desativa o blending do rastro wave." },
    { "Disables player trail for the wave.",
      "Desactiva la estela del jugador en wave.",
      "Disattiva la scia giocatore per la wave.",
      "Desativa o rastro do jogador para a wave." },
    { "Shows your noclip accuracy when completing a level with noclip.",
      "Muestra la precision noclip al completar un nivel con noclip.",
      "Mostra la precisione noclip quando completi un livello con noclip.",
      "Mostra sua precisao noclip ao completar um nivel com noclip." },
};

#define TRANSLATION_COUNT ((int) (sizeof(translations) / sizeof(translations[0])))

static const char *column(const Translation *t, int lang) {
    switch (lang) {
        case LANG_SPANISH:    return t->es;
        case LANG_ITALIAN:    return t->it;
        case LANG_PORTUGUESE: return t->pt;
        default:              return t->en;
    }
}

const char *tr(const char *english) {
    if (!english || language == LANG_ENGLISH) return english;

    for (int i = 0; i < TRANSLATION_COUNT; i++)
        if (!strcmp(translations[i].en, english))
            return column(&translations[i], language);

    return english;
}

void i18n_set_language(int lang) {
    language = (lang >= LANG_ENGLISH && lang <= LANG_PORTUGUESE) ? lang : LANG_ENGLISH;
}

// A text of any of the four languages becomes the same string in the current
// one: switching language works on screens that are already loaded.
static const char *localize_text(const char *text) {
    for (int i = 0; i < TRANSLATION_COUNT; i++) {
        const Translation *t = &translations[i];

        if (!strcmp(t->en, text) || !strcmp(t->es, text) ||
            !strcmp(t->it, text) || !strcmp(t->pt, text))
            return column(t, language);
    }

    return text;
}

static void replace_text(char *dst, size_t size, const char *text) {
    if (dst == text) return;
    strncpy(dst, text, size - 1);
    dst[size - 1] = 0;
}

static void localize_element(UIElement *e) {
    switch (e->type) {
        case UI_LABEL: {
            UILabel *label = (UILabel *) e;
            replace_text(label->text, sizeof(label->text), localize_text(label->text));
            return;
        }
        case UI_BUTTON:
        case UI_CHECKBOX:
        case UI_ICON:
        case UI_COLOR_BUTTON:
        case UI_WINDOW_BUTTON: {
            UIButton *button = (UIButton *) e;
            replace_text(button->text, sizeof(button->text), localize_text(button->text));
            return;
        }
        case UI_TEXTBOX: {
            UITextbox *textbox = (UITextbox *) e;
            replace_text(textbox->text, sizeof(textbox->text), localize_text(textbox->text));
            return;
        }
        default:
            return;
    }
}

static void localize_tree(UIElement *e) {
    while (e) {
        localize_element(e);
        localize_tree(e->first_child);
        e = e->next_sibling;
    }
}

void i18n_localize_screen(struct UIScreen *screen) {
    if (!screen || !screen->elements) return;

    for (size_t i = 0; i < screen->count; i++)
        localize_tree(screen->elements[i]);
}
