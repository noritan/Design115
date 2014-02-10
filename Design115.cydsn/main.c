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

void echoBackUart(void) {
	uint8   rxmessage[64];
	uint8   length;
    uint8   i;

    if (USBUART_DataIsReady()) {                // PCからのデータ受信待ち
        length = USBUART_GetCount();            // 受信したデータ長の取得
        USBUART_GetData(rxmessage, length);     // 受信データを取得

        for (i = 0; i < length; i++) {
            LCD_PutChar(rxmessage[i]);		    // LCDに受信データを表示
        }
		while (!USBUART_CDCIsReady());	        //送信バッファが空か確認
	    USBUART_PutData(rxmessage, length);		//確認用に受信したデータを送信
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
