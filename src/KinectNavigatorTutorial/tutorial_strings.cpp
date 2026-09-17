#include "framework.h"
#include "tutorial_strings.h"

// Source is UTF-8 (built with /utf-8, see KinectNavigatorTutorial.vcxproj) so these wide string
// literals are exactly the characters written below, regardless of BOM.
namespace TutStr
{
    namespace
    {
        Lang g_lang = Lang::En;

        // One row per language, in the exact order of enum S. Keep new entries in sync with
        // both S and this table -- there's no compile-time check tying the two together.
        const wchar_t* const kTable[(int)Lang::Count][(int)S::Count] =
        {
            // ---- En ----
            {
                L"Welcome", L"Step into view, about 2.5 m back, facing the sensor.",
                L"Wake it up", L"Rest your dominant hand near your dominant shoulder and hold it.",
                L"Reach right", L"Reach your dominant hand out to the side.",
                L"Reach left", L"Now the other way.",
                L"Reach up", L"Reach your dominant hand straight up.",
                L"Reach down", L"Reach down and OUT to the side -- straight down does nothing.",
                L"Command mode", L"Bring your OTHER hand to your OTHER shoulder and hold it there.",
                L"Confirm", L"Gate held: reach your dominant hand up or right and hold -- that's Enter.",
                L"Back", L"Gate held: reach your dominant hand down and out, and hold longer -- that's Esc.",
                L"All set", L"That's the whole vocabulary. Close this window whenever you're ready.",
                L"Connecting to your Kinect...", L"Kinect software not found", L"Kinect not detected",
                L"Install the Kinect for Windows Runtime (or SDK 1.8), then this will pick it up automatically.",
                L"Plug it in (and make sure the Kinect service is running) -- retrying automatically...",
                L"Just a moment...", L"Esc to close",
                L"reference", L"you", L"step into view",
                L"Kinect disconnected -- reconnect to continue",
                L"Space to skip this step · Esc to close",
                L"KinectNavigator -- Try it out",
            },
            // ---- Fr ----
            {
                L"Bienvenue", L"Placez-vous dans le champ de vision, à environ 2,5 m, face au capteur.",
                L"Réveillez-le", L"Posez votre main dominante près de votre épaule dominante et maintenez-la là.",
                L"Tendez le bras à droite", L"Tendez votre main dominante sur le côté.",
                L"Tendez le bras à gauche", L"Maintenant dans l'autre sens.",
                L"Tendez le bras vers le haut", L"Tendez votre main dominante droit vers le haut.",
                L"Tendez le bras vers le bas", L"Tendez la main vers le bas ET vers l'extérieur -- tout droit vers le bas ne fait rien.",
                L"Mode commande", L"Amenez votre AUTRE main sur votre AUTRE épaule et maintenez-la là.",
                L"Confirmer", L"Portail maintenu : tendez la main dominante vers le haut ou la droite et maintenez -- c'est Entrée.",
                L"Retour", L"Portail maintenu : tendez la main dominante vers le bas et l'extérieur, et maintenez plus longtemps -- c'est Échap.",
                L"C'est prêt", L"Voilà tout le vocabulaire. Fermez cette fenêtre quand vous voulez.",
                L"Connexion à votre Kinect...", L"Logiciel Kinect introuvable", L"Kinect non détectée",
                L"Installez le Kinect for Windows Runtime (ou le SDK 1.8) ; la détection se fera alors automatiquement.",
                L"Branchez-la (et vérifiez que le service Kinect est actif) -- nouvel essai automatique...",
                L"Un instant...", L"Échap pour fermer",
                L"référence", L"vous", L"entrez dans le champ",
                L"Kinect déconnectée -- reconnectez-la pour continuer",
                L"Espace pour passer cette étape · Échap pour fermer",
                L"KinectNavigator — Essayez-le",
            },
            // ---- Es ----
            {
                L"Bienvenido", L"Ponte a la vista, a unos 2,5 m, de frente al sensor.",
                L"Despiértalo", L"Apoya tu mano dominante cerca de tu hombro dominante y mantenla ahí.",
                L"Extiende hacia la derecha", L"Extiende tu mano dominante hacia el lado.",
                L"Extiende hacia la izquierda", L"Ahora hacia el otro lado.",
                L"Extiende hacia arriba", L"Extiende tu mano dominante recto hacia arriba.",
                L"Extiende hacia abajo", L"Extiende hacia abajo y HACIA AFUERA -- hacia abajo recto no hace nada.",
                L"Modo de comando", L"Lleva tu OTRA mano a tu OTRO hombro y mantenla ahí.",
                L"Confirmar", L"Con la puerta activada: extiende la mano dominante hacia arriba o a la derecha y mantenla -- eso es Intro.",
                L"Atrás", L"Con la puerta activada: extiende la mano dominante hacia abajo y afuera, y mantenla más tiempo -- eso es Esc.",
                L"Listo", L"Eso es todo el vocabulario. Cierra esta ventana cuando quieras.",
                L"Conectando con tu Kinect...", L"Software de Kinect no encontrado", L"Kinect no detectado",
                L"Instala el Kinect for Windows Runtime (o el SDK 1.8) y lo detectará automáticamente.",
                L"Conéctalo (y asegúrate de que el servicio de Kinect esté activo) -- reintentando automáticamente...",
                L"Un momento...", L"Esc para cerrar",
                L"referencia", L"tú", L"ponte a la vista",
                L"Kinect desconectado -- vuelve a conectarlo para continuar",
                L"Espacio para saltar este paso · Esc para cerrar",
                L"KinectNavigator — Pruébalo",
            },
            // ---- De ----
            {
                L"Willkommen", L"Stell dich vor den Sensor, etwa 2,5 m entfernt, mit Blick darauf.",
                L"Wecken", L"Lege deine dominante Hand nahe an deine dominante Schulter und halte sie dort.",
                L"Nach rechts greifen", L"Strecke deine dominante Hand zur Seite aus.",
                L"Nach links greifen", L"Jetzt die andere Richtung.",
                L"Nach oben greifen", L"Strecke deine dominante Hand gerade nach oben.",
                L"Nach unten greifen", L"Greife nach unten UND nach außen -- gerade nach unten bewirkt nichts.",
                L"Befehlsmodus", L"Bringe deine ANDERE Hand an deine ANDERE Schulter und halte sie dort.",
                L"Bestätigen", L"Bei gehaltenem Gate: Strecke die dominante Hand nach oben oder rechts und halte sie -- das ist Enter.",
                L"Zurück", L"Bei gehaltenem Gate: Strecke die dominante Hand nach unten und außen, und halte länger -- das ist Esc.",
                L"Fertig", L"Das war das ganze Vokabular. Schließe dieses Fenster, wann immer du möchtest.",
                L"Verbindung zu deinem Kinect wird hergestellt...", L"Kinect-Software nicht gefunden", L"Kein Kinect erkannt",
                L"Installiere die Kinect for Windows Runtime (oder das SDK 1.8), dann wird es automatisch erkannt.",
                L"Schließe es an (und stelle sicher, dass der Kinect-Dienst läuft) -- automatischer erneuter Versuch...",
                L"Einen Moment...", L"Esc zum Schließen",
                L"Referenz", L"du", L"in den Sichtbereich treten",
                L"Kinect getrennt -- erneut verbinden, um fortzufahren",
                L"Leertaste überspringt diesen Schritt · Esc schließt",
                L"KinectNavigator — Ausprobieren",
            },
            // ---- It ----
            {
                L"Benvenuto", L"Mettiti davanti al sensore, a circa 2,5 m, rivolto verso di esso.",
                L"Sveglialo", L"Appoggia la mano dominante vicino alla spalla dominante e tienila lì.",
                L"Tendi verso destra", L"Allunga la mano dominante lateralmente.",
                L"Tendi verso sinistra", L"Ora dall'altra parte.",
                L"Tendi verso l'alto", L"Allunga la mano dominante dritta verso l'alto.",
                L"Tendi verso il basso", L"Allunga la mano verso il basso E verso l'esterno -- dritto verso il basso non fa nulla.",
                L"Modalità comando", L"Porta l'ALTRA mano sull'ALTRA spalla e tienila lì.",
                L"Conferma", L"Con il gate attivo: allunga la mano dominante verso l'alto o a destra e tienila -- equivale a Invio.",
                L"Indietro", L"Con il gate attivo: allunga la mano dominante verso il basso e l'esterno, tenendola più a lungo -- equivale a Esc.",
                L"Tutto pronto", L"Questo è tutto il vocabolario. Chiudi questa finestra quando vuoi.",
                L"Connessione al tuo Kinect...", L"Software Kinect non trovato", L"Kinect non rilevato",
                L"Installa il Kinect for Windows Runtime (o l'SDK 1.8): verrà rilevato automaticamente.",
                L"Collegalo (e assicurati che il servizio Kinect sia attivo) -- nuovo tentativo automatico...",
                L"Un momento...", L"Esc per chiudere",
                L"riferimento", L"tu", L"mettiti davanti al sensore",
                L"Kinect disconnesso -- ricollega per continuare",
                L"Spazio per saltare questo passaggio · Esc per chiudere",
                L"KinectNavigator — Provalo",
            },
            // ---- Ja ----
            {
                L"ようこそ", L"センサーの正面、約2.5m後ろに立ってください。",
                L"起動する", L"利き手を利き手側の肩の近くに置き、そのまま保持します。",
                L"右に伸ばす", L"利き手を横に伸ばします。",
                L"左に伸ばす", L"今度は逆方向です。",
                L"上に伸ばす", L"利き手をまっすぐ上に伸ばします。",
                L"下に伸ばす", L"下かつ外側に伸ばします -- 真下だけでは反応しません。",
                L"コマンドモード", L"反対の手を反対側の肩に当てて、そのまま保持します。",
                L"確定", L"ゲートを保持したまま、利き手を上か右に伸ばして保持 -- Enterに相当します。",
                L"戻る", L"ゲートを保持したまま、利き手を下かつ外側に伸ばして長めに保持 -- Escに相当します。",
                L"完了", L"これで一通りの動作は完了です。準備ができたらこのウィンドウを閉じてください。",
                L"Kinect に接続しています...", L"Kinect のソフトウェアが見つかりません", L"Kinect が検出されません",
                L"Kinect for Windows Runtime (または SDK 1.8) をインストールすると自動的に検出されます。",
                L"接続してください (Kinect サービスが実行中か確認) -- 自動的に再試行します...",
                L"少々お待ちください...", L"Esc で閉じる",
                L"お手本", L"あなた", L"センサーの前に立ってください",
                L"Kinect が切断されました -- 再接続すると続行できます",
                L"Space でこのステップをスキップ · Esc で閉じる",
                L"KinectNavigator — 試してみる",
            },
            // ---- Ko ----
            {
                L"환영합니다", L"센서에서 약 2.5m 떨어져 정면을 보고 서세요.",
                L"깨우기", L"주로 쓰는 손을 주로 쓰는 어깨 근처에 대고 그대로 유지하세요.",
                L"오른쪽으로 뻗기", L"주로 쓰는 손을 옆으로 뻗으세요.",
                L"왼쪽으로 뻗기", L"이번엔 반대쪽입니다.",
                L"위로 뻗기", L"주로 쓰는 손을 똑바로 위로 뻗으세요.",
                L"아래로 뻗기", L"아래쪽과 바깥쪽으로 뻗으세요 -- 그냥 아래로만 뻗으면 반응하지 않습니다.",
                L"명령 모드", L"다른 쪽 손을 다른 쪽 어깨에 대고 그대로 유지하세요.",
                L"확인", L"게이트를 유지한 상태로 주로 쓰는 손을 위나 오른쪽으로 뻗어 유지하세요 -- Enter에 해당합니다.",
                L"뒤로", L"게이트를 유지한 상태로 주로 쓰는 손을 아래쪽과 바깥쪽으로 뻗어 더 오래 유지하세요 -- Esc에 해당합니다.",
                L"완료", L"이것으로 전체 동작을 모두 배웠습니다. 준비되면 이 창을 닫으세요.",
                L"Kinect에 연결하는 중...", L"Kinect 소프트웨어를 찾을 수 없습니다", L"Kinect가 감지되지 않습니다",
                L"Kinect for Windows Runtime (또는 SDK 1.8)을 설치하면 자동으로 인식됩니다.",
                L"연결하세요 (Kinect 서비스가 실행 중인지 확인) -- 자동으로 다시 시도합니다...",
                L"잠시만요...", L"Esc로 닫기",
                L"참고 동작", L"나", L"센서 앞에 서세요",
                L"Kinect 연결이 끊어졌습니다 -- 다시 연결하면 계속할 수 있습니다",
                L"Space로 이 단계 건너뛰기 · Esc로 닫기",
                L"KinectNavigator — 체험하기",
            },
            // ---- Nl ----
            {
                L"Welkom", L"Ga voor de sensor staan, ongeveer 2,5 m erachter, met je gezicht ernaartoe.",
                L"Maak het wakker", L"Leg je dominante hand bij je dominante schouder en houd hem daar.",
                L"Reik naar rechts", L"Strek je dominante hand opzij.",
                L"Reik naar links", L"Nu de andere kant op.",
                L"Reik omhoog", L"Strek je dominante hand recht omhoog.",
                L"Reik omlaag", L"Reik omlaag EN naar buiten -- recht omlaag doet niets.",
                L"Commandomodus", L"Breng je ANDERE hand naar je ANDERE schouder en houd hem daar.",
                L"Bevestigen", L"Terwijl de poort vastgehouden wordt: strek je dominante hand omhoog of naar rechts en houd vast -- dat is Enter.",
                L"Terug", L"Terwijl de poort vastgehouden wordt: strek je dominante hand omlaag en naar buiten, en houd langer vast -- dat is Esc.",
                L"Helemaal klaar", L"Dat is de hele woordenschat. Sluit dit venster wanneer je wilt.",
                L"Verbinden met je Kinect...", L"Kinect-software niet gevonden", L"Geen Kinect gedetecteerd",
                L"Installeer de Kinect for Windows Runtime (of SDK 1.8), dan wordt hij automatisch herkend.",
                L"Sluit hem aan (en zorg dat de Kinect-service actief is) -- probeert automatisch opnieuw...",
                L"Een ogenblikje...", L"Esc om te sluiten",
                L"referentie", L"jij", L"ga voor de sensor staan",
                L"Kinect losgekoppeld -- maak opnieuw verbinding om door te gaan",
                L"Spatie om deze stap over te slaan · Esc om te sluiten",
                L"KinectNavigator — Probeer het",
            },
            // ---- Pt ----
            {
                L"Bem-vindo", L"Fique à vista, a cerca de 2,5 m, de frente para o sensor.",
                L"Acorde-o", L"Apoie a mão dominante perto do ombro dominante e mantenha-a lá.",
                L"Estenda para a direita", L"Estenda a mão dominante para o lado.",
                L"Estenda para a esquerda", L"Agora para o outro lado.",
                L"Estenda para cima", L"Estenda a mão dominante reto para cima.",
                L"Estenda para baixo", L"Estenda para baixo E para fora -- reto para baixo não faz nada.",
                L"Modo de comando", L"Leve a OUTRA mão ao OUTRO ombro e mantenha-a lá.",
                L"Confirmar", L"Com o portão ativo: estenda a mão dominante para cima ou para a direita e segure -- equivale a Enter.",
                L"Voltar", L"Com o portão ativo: estenda a mão dominante para baixo e para fora, segurando por mais tempo -- equivale a Esc.",
                L"Tudo pronto", L"Esse é todo o vocabulário. Feche esta janela quando quiser.",
                L"Conectando ao seu Kinect...", L"Software do Kinect não encontrado", L"Kinect não detectado",
                L"Instale o Kinect for Windows Runtime (ou o SDK 1.8) e ele será detectado automaticamente.",
                L"Conecte-o (e confirme que o serviço do Kinect está em execução) -- tentando novamente de forma automática...",
                L"Um momento...", L"Esc para fechar",
                L"referência", L"você", L"fique à vista do sensor",
                L"Kinect desconectado -- reconecte para continuar",
                L"Espaço para pular esta etapa · Esc para fechar",
                L"KinectNavigator — Experimentar",
            },
            // ---- Ru ----
            {
                L"Добро пожаловать", L"Встаньте в поле зрения датчика, примерно в 2,5 м, лицом к нему.",
                L"Разбудите его", L"Поднесите ведущую руку к ведущему плечу и удерживайте её там.",
                L"Вытяните руку вправо", L"Вытяните ведущую руку в сторону.",
                L"Вытяните руку влево", L"Теперь в другую сторону.",
                L"Вытяните руку вверх", L"Вытяните ведущую руку прямо вверх.",
                L"Вытяните руку вниз", L"Вытяните руку вниз И в сторону -- просто вниз ничего не даст.",
                L"Режим команд", L"Поднесите ДРУГУЮ руку к ДРУГОМУ плечу и удерживайте её там.",
                L"Подтвердить", L"Удерживая ворота: вытяните ведущую руку вверх или вправо и удержите -- это Enter.",
                L"Назад", L"Удерживая ворота: вытяните ведущую руку вниз и в сторону, удерживая дольше -- это Esc.",
                L"Готово", L"Это весь набор жестов. Закройте это окно, когда будете готовы.",
                L"Подключение к Kinect...", L"Программное обеспечение Kinect не найдено", L"Kinect не обнаружен",
                L"Установите Kinect for Windows Runtime (или SDK 1.8), и он определится автоматически.",
                L"Подключите его (и убедитесь, что служба Kinect запущена) -- автоматическая повторная попытка...",
                L"Один момент...", L"Esc, чтобы закрыть",
                L"образец", L"вы", L"встаньте в поле зрения",
                L"Kinect отключён -- переподключите, чтобы продолжить",
                L"Пробел -- пропустить шаг · Esc -- закрыть",
                L"KinectNavigator — Попробовать",
            },
            // ---- ZhHans ----
            {
                L"欢迎", L"站到传感器前方约2.5米处，正对它。",
                L"唤醒", L"将惯用手放在惯用侧肩膀附近并保持不动。",
                L"向右伸出", L"将惯用手向侧面伸出。",
                L"向左伸出", L"现在换另一个方向。",
                L"向上伸出", L"将惯用手笔直向上伸出。",
                L"向下伸出", L"向下并向外伸出 -- 只是笔直向下不会有反应。",
                L"命令模式", L"将另一只手放到另一侧肩膀并保持不动。",
                L"确认", L"保持手势门开启：将惯用手向上或向右伸出并保持 -- 相当于回车键。",
                L"返回", L"保持手势门开启：将惯用手向下并向外伸出，保持更长时间 -- 相当于 Esc 键。",
                L"全部完成", L"这就是全部动作。准备好后随时可以关闭此窗口。",
                L"正在连接 Kinect...", L"未找到 Kinect 软件", L"未检测到 Kinect",
                L"安装 Kinect for Windows 运行库 (或 SDK 1.8) 后将自动识别。",
                L"请连接它 (并确认 Kinect 服务正在运行) -- 正在自动重试...",
                L"请稍候...", L"按 Esc 关闭",
                L"示范", L"你", L"请站到传感器前方",
                L"Kinect 已断开 -- 重新连接后可继续",
                L"空格跳过此步骤 · Esc 关闭",
                L"KinectNavigator — 试用",
            },
            // ---- ZhHant ----
            {
                L"歡迎", L"站到感應器前方約2.5公尺處，正對它。",
                L"喚醒", L"將慣用手放在慣用側肩膀附近並保持不動。",
                L"向右伸出", L"將慣用手向側面伸出。",
                L"向左伸出", L"現在換另一個方向。",
                L"向上伸出", L"將慣用手筆直向上伸出。",
                L"向下伸出", L"向下並向外伸出 -- 只是筆直向下不會有反應。",
                L"指令模式", L"將另一隻手放到另一側肩膀並保持不動。",
                L"確認", L"保持手勢閘門開啟：將慣用手向上或向右伸出並保持 -- 相當於 Enter 鍵。",
                L"返回", L"保持手勢閘門開啟：將慣用手向下並向外伸出，保持更長時間 -- 相當於 Esc 鍵。",
                L"全部完成", L"這就是全部動作。準備好後隨時可以關閉此視窗。",
                L"正在連接 Kinect...", L"找不到 Kinect 軟體", L"未偵測到 Kinect",
                L"安裝 Kinect for Windows Runtime (或 SDK 1.8) 後將自動偵測。",
                L"請連接它 (並確認 Kinect 服務正在執行) -- 正在自動重試...",
                L"請稍候...", L"按 Esc 關閉",
                L"示範", L"你", L"請站到感應器前方",
                L"Kinect 已中斷連線 -- 重新連接後可繼續",
                L"空白鍵跳過此步驟 · Esc 關閉",
                L"KinectNavigator — 試用",
            },
        };
    }

    Lang ParseLangCode(const wchar_t* code)
    {
        if (!code || !code[0]) return Lang::En;
        struct Entry { const wchar_t* code; Lang lang; };
        static const Entry kMap[] = {
            { L"en", Lang::En }, { L"fr", Lang::Fr }, { L"es", Lang::Es }, { L"de", Lang::De },
            { L"it", Lang::It }, { L"ja", Lang::Ja }, { L"ko", Lang::Ko }, { L"nl", Lang::Nl },
            { L"pt", Lang::Pt }, { L"ru", Lang::Ru },
            { L"zh-Hans", Lang::ZhHans }, { L"zh-Hant", Lang::ZhHant },
        };
        for (const auto& e : kMap)
            if (_wcsicmp(code, e.code) == 0) return e.lang;
        return Lang::En;
    }

    void SetLang(Lang l) { g_lang = l; }

    const wchar_t* T(S id)
    {
        const int li = (int)g_lang, si = (int)id;
        const wchar_t* s = kTable[li][si];
        if (s && s[0]) return s;
        return kTable[(int)Lang::En][si];   // fall back to English for any gap
    }
}
