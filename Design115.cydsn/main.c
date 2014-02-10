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

static void putch_sub(const int16 ch) {
    uint8   txBuffer[1];
    
    txBuffer[0] = ch;
	while (!USBUART_CDCIsReady());	            // 送信バッファが空か確認
    USBUART_PutData(txBuffer, 1);		        // 確認用に受信したデータを送信
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
    int16   ch = -1;
	uint8   rxmessage[64];
	uint8   length;

    if (USBUART_DataIsReady()) {                // PCからのデータ受信待ち
        length = USBUART_GetCount();            // 受信したデータ長の取得
        USBUART_GetData(rxmessage, length);     // 受信データを取得
        ch = rxmessage[0];
    }
    return ch;
}

// USBUARTのエコーバック処理
void echoBackUart(void) {
    int16       ch;
    
    if ((ch = getch()) >= 0) {
        LCD_PutChar(ch);                        // LCDに受信データを表示
        putch(ch);                              // 確認用に受信したデータを送信
    }
}

int main()
{
	CyGlobalIntEnable;                          // 割り込みの有効化    
	LCD_Start();                                // LCDの初期化
    USBUART_Start(0, USBUART_5V_OPERATION);     // 動作電圧5VにてUSBFSコンポーネントを初期化

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
