/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include <project.h>

// MSCインターフェイスのエンドポイント番号
#define     MSC_IN      (4)
#define     MSC_OUT     (5)

// USBUARTのパケットサイズ
#define     UART_TX_QUEUE_SIZE      (64)
#define     UART_RX_QUEUE_SIZE      (64)

// USBUARTのTXキューバッファ
uint8       uartTxQueue[UART_TX_QUEUE_SIZE];    // TXキュー
uint8       uartTxCount = 0;                    // TXキューに存在するデータ数
CYBIT       uartZlpRequired = 0;                // 要ZLPフラグ

// USBUARTのRXキューバッファ
uint8       uartRxQueue[UART_RX_QUEUE_SIZE];    // RXキュー
uint8       uartRxCount = 0;                    // RXキューに存在するデータ数
uint8       uartRxIndex = 0;                    // RXキューからの取り出し位置

// 周期的にUSBUARTの送受信を監視する
CY_ISR(int_uartQueue_isr) {
    // 送信制御
    if ((uartTxCount > 0) || uartZlpRequired) {
        // バッファのデータを出力する
        if (USBUART_CDCIsReady()) {
            USBUART_PutData(uartTxQueue, uartTxCount);
            uartZlpRequired = (uartTxCount == UART_TX_QUEUE_SIZE);
            uartTxCount = 0;
        }
    }
    // 受信制御
    if (uartRxIndex >= uartRxCount) {
        // 空の入力バッファにデータを受け取る
        if (USBUART_DataIsReady()) {
            uartRxCount = USBUART_GetAll(uartRxQueue);
            uartRxIndex = 0;
        }
    }
}

static void putch_sub(const int16 ch) {
    for (;;) {                                  // 送信キューが空くまで待つ
        int_uartQueue_Disable();
        if (uartTxCount < UART_TX_QUEUE_SIZE) break;
        int_uartQueue_Enable();
    }
    uartTxQueue[uartTxCount++] = ch;            // 送信キューに一文字入れる
    int_uartQueue_Enable();
}

// USBUARTに一文字送る
void putch(const int16 ch) {
    if (ch == '\n') {
        putch_sub('\r');
    }
    putch_sub(ch);
}

// USBUARTから一文字受け取る
int16 getch(void) {
    int16 ch = -1;
    
    int_uartQueue_Disable();
    if (uartRxIndex < uartRxCount) {            // 受信キューに文字があるか確認
        ch = uartRxQueue[uartRxIndex++];        // 受信キューから一文字取り出す
        if (ch == '\r') {                       // 行末文字の変換処理
            ch = '\n';
        }
    }
    int_uartQueue_Enable();
    return ch;
}

// 32-bit十進数表
static const uint32 CYCODE pow10_32[] = {
    0L,
    1L,
    10L,
    100L,
    1000L,
    10000L,
    100000L,
    1000000L,
    10000000L,
    100000000L,
    1000000000L,
};

// 32-bit数値の十進表示 - ZERO SUPPRESS は省略。
void putdec32(uint32 num, const uint8 nDigits) {
    uint8       i;
    uint8       k;
    CYBIT       show = 0;

    // 表示すべき桁数
    i = sizeof pow10_32 / sizeof pow10_32[0];
    while (--i > 0) {             // 一の位まで表示する
        // i桁目の数値を得る
        for (k = 0; num >= pow10_32[i]; k++) {
            num -= pow10_32[i];
        }
        // 表示すべきか判断する
        show = show || (i <= nDigits) || (k != 0);
        // 必要なら表示する
        if (show) {
            putch(k + '0');     // 着目桁の表示
        }
    }
}

// 16-bit十進数表
static const uint16 CYCODE pow10_16[] = {
    0L,
    1L,
    10L,
    100L,
    1000L,
    10000L,
};

// 16-bit数値の十進表示 - ZERO SUPPRESS は省略。
void putdec16(uint16 num, const uint8 nDigits) {
    uint8       i;
    uint8       k;
    CYBIT       show = 0;

    // 表示すべき桁数
    i = sizeof pow10_16 / sizeof pow10_16[0];
    while (--i > 0) {             // 一の位まで表示する
        // i桁目の数値を得る
        for (k = 0; num >= pow10_16[i]; k++) {
            num -= pow10_16[i];
        }
        // 表示すべきか判断する
        show = show || (i <= nDigits) || (k != 0);
        // 必要なら表示する
        if (show) {
            putch(k + '0');     // 着目桁の表示
        }
    }
}

// 十六進数表
static char8 const CYCODE hex[] = "0123456789ABCDEF";

// 8-bit十六進数の表示
void puthex8(uint8 num) {
    putch(hex[num >> 4]);
    putch(hex[num & 0x0F]);
}

// 16-bit十六進数の表示
void puthex16(uint16 num) {
    puthex8(num >> 8);
    puthex8(num);
}

// USBUARTに文字列を送り込む
void putstr(const char *s) {
    // 行末まで表示する
    while (*s) {
        putch(*s++);
    }
}

static uint16       lineCount = 0;

// USBUARTのエコーバック処理
void echoBackUart(void) {
    int16       ch;
    
    if ((ch = getch()) >= 0) {
        if (ch == '\014') {                     // ctrl+L
            LCD_ClearDisplay();                 // LCDをクリア
        } else {
            LCD_PutChar(ch);                    // LCDに受信データを表示
        }
        putch(ch);                              // 確認用に受信したデータを送信
        if (ch == '\n') {
            puthex16(lineCount);                // 改行ごとに十六進の数値を表示する
            putstr("-");                        // 区切り文字
            putdec16(lineCount++, 3);           // 改行ごとに数値を表示する
            putstr(" %");                       // プロンプトの表示
        }
    }
}

int main()
{
    CyGlobalIntEnable;                          // 割り込みの有効化    
    LCD_Start();                                // LCDの初期化
    USBUART_Start(0, USBUART_5V_OPERATION);     // 動作電圧5VにてUSBFSコンポーネントを初期化
    int_uartQueue_StartEx(int_uartQueue_isr);   // 周期割り込みの設定

    for (;;) {
        // 初期化終了まで待機
        while (USBUART_GetConfiguration() == 0);

        USBUART_IsConfigurationChanged();       // CHANGEフラグを確実にクリアする

        USBUART_CDC_Init();                     // USBFSのCDCコンポーネントを初期化

        for (;;) {
            // 設定が変更されたら、再初期化をおこなう
            if (USBUART_IsConfigurationChanged()) {
                break;
            }
            
            // UARTのエコーバック処理
            echoBackUart();

            // SWをLEDにコピーする処理
            LED4_Write(SW2_Read());
            LED3_Write(SW3_Read());
        }
    }
}

/* [] END OF FILE */
