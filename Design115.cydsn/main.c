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

uint8       uartTxReject = 0;                   // 送信不可回数

// 周期的にUSBUARTの送受信を監視する
CY_ISR(int_uartQueue_isr) {
    // 送信制御
    if ((uartTxCount > 0) || uartZlpRequired) {
        // バッファのデータを出力する
        if (USBUART_CDCIsReady()) {
            USBUART_PutData(uartTxQueue, uartTxCount);
            uartZlpRequired = (uartTxCount == UART_TX_QUEUE_SIZE);
            uartTxCount = 0;
            uartTxReject = 0;
        } else if (++uartTxReject > 4) {
            // バッファのデータを棄てる
            uartTxCount = 0;
            uartTxReject = 0;
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

// 32-bit十六進数の表示
void puthex32(uint32 num) {
    puthex16(num >> 16);
    puthex16(num);
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

// MSCデバイスの定数宣言
#define         EP_SIZE         (64)

// MSCデバイスのステートマシン宣言
typedef enum MscState_ {
    MSCST_CBW_WAIT = 0,                 // CBWパケット待ち
    MSCST_PREPARE,                      // コマンドの準備
    MSCST_IN_WAIT,                      // BULK-IN送信待ち
    MSCST_IN_SET,                       // BULK-INデータセット
    MSCST_IN_COMPLETE,                  // BULK-IN送信完了
    MSCST_READ_GET,                     // READデータ読み出し待ち
    MSCST_READ_LOAD,                    // READデータ送信待ち
    MSCST_WRITE_READ,                   // WRITEデータ受信待ち
    MSCST_WRITE_PUT,                    // WRITEデータ書き込み待ち
    MSCST_IN_LOAD,                      // IN応答送信待ち
    MSCST_CSW_PREPARE,                  // CSW準備
    MSCST_CSW_WAIT,                     // CSW送信待ち
    MSCST_CSW_SET,                      // CSWデータセット
    MSCST_CSW_COMPLETE,                 // CSW送信完了
}   MscState;

MscState    mscState;                   // 状態変数

// SCSIコマンド
enum ScsiCommand {
    SCSI_TEST_UNIT_READY = 0x00,        // TEST UNIT READY
    SCSI_REQUEST_SENSE = 0x03,          // REQUEST SENSE
    SCSI_INQUIRY = 0x12,                // INQUIRY
    SCSI_READ_CAPACITY_10 = 0x25,       // READ CAPACITY(10)
    SCSI_READ_10 = 0x28,                // READ(10)
};

// MSCデバイスの初期設定
void mscInit(void) {
    putstr("\n\n*** MSC Started ***\n");        // Opening message
    USBUART_EnableOutEP(MSC_OUT);               // MSCのEndpointを開く
    mscState = MSCST_CBW_WAIT;                  // MSCデバイスの初期状態
}

uint16      mscCbwLength;                       // CBWの長さ
uint8       cbw[31];                            // CBWの受信バッファ
uint8       csw[13] = {'U', 'S', 'B', 'S'};     // CSWの送信バッファ
uint32      mscCbwDataTransferLength;           // CBWのデータ転送長
uint8       mscBufferIn[64];                    // Bulk-in転送のバッファ
uint32      mscBufferInLength;                  // Bulk-inバッファ内のデータ長
uint32      mscScsiAddress;                     // 外部記憶アドレスポインタ

// CBWフィールドから32ビットの値を取り出す
uint32 mscCbwGetValue32(uint8 *s) {
    return ((((uint32)((((uint16)s[3])<<8)|s[2])<<8)|s[1])<<8)|s[0];
}

// SCSIフィールドから32ビットの値を取り出す
uint32 mscScsiGetValue32(uint8 *s) {
    return ((((uint32)((((uint16)s[0])<<8)|s[1])<<8)|s[2])<<8)|s[3];
}

// SCSIフィールドから16ビットの値を取り出す
uint16 mscScsiGetValue16(uint8 *s) {
    return (((uint16)s[0])<<8)|s[1];
}

// フィールドに32ビットの値を書込む
void mscPutValue32(uint8 *p, uint32 v) {
    p[0] = v & 0x000000FF;
    v >>= 8;
    p[1] = v & 0x000000FF;
    v >>= 8;
    p[2] = v & 0x000000FF;
    v >>= 8;
    p[3] = v & 0x000000FF;
}

// CBWを表示する
void mscShowCbw(void) {
    uint8       i;
    
    putstr("\nCBW:");
    for (i = 0; i < mscCbwLength; i++) {
        putch(' ');
        puthex8(cbw[i]);
    }
}

// CBWは有効か?
uint8 mscCbwIsValid(void) {
    if (mscCbwLength != 31) return 0;
    if (cbw[0] != 'U') return 0;        // Signature
    if (cbw[1] != 'S') return 0;
    if (cbw[2] != 'B') return 0;
    if (cbw[3] != 'C') return 0;
    if (cbw[12] & 0x7F) return 0;       // Flag[6:0]=0
    if (cbw[13] != 0) return 0;         // LUN=0
    // CBLength
    if ((cbw[14] < 1)||(cbw[14] > 16)) return 0;
    return 1;
}

static uint8 senseData[18];

void mscScsiSenseDataInit(void) {
    senseData[ 0] = 0x00;               // RESPONSE CODE
    senseData[ 1] = 0x00;               // Obsoleted
    senseData[ 2] = 0x00;
    senseData[ 3] = 0x00;
    senseData[ 4] = 0x00;
    senseData[ 5] = 0x00;
    senseData[ 6] = 0x00;
    senseData[ 7] = 0x00;
    senseData[ 8] = 0x00;
    senseData[ 9] = 0x00;
    senseData[10] = 0x00;
    senseData[11] = 0x00;
    senseData[12] = 0x00;
    senseData[13] = 0x00;
    senseData[14] = 0x00;
    senseData[15] = 0x00;
    senseData[16] = 0x00;
    senseData[17] = 0x00;
}

static uint8 const CYCODE inquiryData[36u] = {
    0x00,               // PERIPHERAL
    0x80,               // RMB = removable
    0x00,               // Version: No conformance claim to standard
    0x02,               // NORMACA=0, HISUP=0, FROMAT=2
    0x20,               // ADDITIONAL 32 Bytes
    0x80,               // SCCS = 1: Storage Controller Component
    0x00,
    0x00,
    'n', 'o', 'r', 'i', 't', 'a', 'n', ' ',     // Vender
    'P', 'S', 'o', 'C', ' ', 'M', 'S', 'C',     // Product
    ' ', 'D', 'r', 'i', 'v', 'e', ' ', ' ',
    '1', '.', '0', '0'                          // Revision
};

// INQUIRYコマンド応答
void mscScsiInquiryPrepare(void) {
    uint8       i;
    
    mscScsiSenseDataInit();
    mscBufferInLength = sizeof inquiryData;
    for (i = 0; i < sizeof inquiryData; i++) {
        mscBufferIn[i] = inquiryData[i];
    }
    mscState = MSCST_IN_WAIT;
}

// 未対応命令へのエラー応答
void mscScsiInvalidCommand(void) {
    senseData[ 0] = 0x70;               // Response Code = Current error
    senseData[ 2] = 0x05;               // SenseKey = ILLEGAL REQUEST
    senseData[12] = 0x20;               // ASC = INVALID COMMAND OPCODE
    senseData[13] = 0x00;               // ASCQ = INVALID COMMAND OPCODE
}

// REQUEST SENSEコマンド応答
void mscScsiRequestSensePrepare(void) {
    uint8       i;
    
    mscBufferInLength = sizeof senseData;
    for (i = 0; i < sizeof senseData; i++) {
        mscBufferIn[i] = senseData[i];
    }
    mscState = MSCST_IN_WAIT;
}

// CAPACITY 256Byte*1024Block=256kByte
static uint8 const CYCODE capacityData[8] = {
    0x00, 0x00, 0x03, 0xFF,             // 1024Blocks
    0x00, 0x00, 0x01, 0x00,             // 256Bytes/Block
};
#define         SECTOR_SIZE             (256)

// CAPACITYデータの準備
void mscScsiReadCapacityPrepare(void) {
    uint8       i;
    
    mscScsiSenseDataInit();
    mscBufferInLength = sizeof capacityData;
    for (i = 0; i < sizeof capacityData; i++) {
        mscBufferIn[i] = capacityData[i];
    }
    if (mscCbwDataTransferLength > mscBufferInLength) {
        csw[12] = 1;
    }
    mscState = MSCST_IN_WAIT;
}

// 未対応IN命令応答
void mscScsiUnknownInPrepare(void) {
    mscScsiSenseDataInit();
    mscScsiInvalidCommand();
    mscBufferInLength = 0;              // Nullパケット
    csw[12] = 1;                        // Command failed
    mscState = MSCST_IN_WAIT;
}

// バッファ内のデータを一回で送信できるコマンド
void mscScsiBufferInSet(void) {
    uint8       size;
    
    // 送信すべきデータ長の計算
    if (mscCbwDataTransferLength >= mscBufferInLength) {
        size = mscBufferInLength;
    } else {
        size = mscCbwDataTransferLength;
    }
    // バッファを送信する
    USBUART_LoadInEP(MSC_IN, &mscBufferIn[0], size);
    putstr("\nSend BULK-IN : ");
    putdec16(size, 1);
    mscCbwDataTransferLength -= size;
    mscBufferInLength -= size;
    // データ長の矛盾
    if ((mscCbwDataTransferLength > 0) || (mscBufferInLength > 0)) {
        csw[12] = 1;
    }
    mscState = MSCST_CSW_PREPARE;
}

// READ(10)コマンド応答
void mscScsiRead10Prepare(void) {
    uint32      lba;
    
    mscScsiSenseDataInit();
    lba = mscScsiGetValue32(&cbw[17]);
    putstr("\nLBA=");
    puthex32(lba);
    mscScsiAddress = lba * SECTOR_SIZE;
    mscBufferInLength = mscScsiGetValue16(&cbw[22]) * SECTOR_SIZE;
    if (mscBufferInLength > mscCbwDataTransferLength) {
        mscScsiInvalidCommand();
        mscBufferInLength = 0;
        csw[12] = 1;
    }
    mscState = MSCST_IN_WAIT;
}

void mscScsiRead10Set(void) {
    uint8       size;
    uint8       i;
    
    // 送信すべきデータ長の計算
    if (mscBufferInLength >= EP_SIZE) {
        size = EP_SIZE;
    } else {
        size = mscBufferInLength;
    }
    // Null sector
    for (i = 0; i < size; i++) {
        mscBufferIn[i] = 0x00;
    }
    // バッファを送信する
    USBUART_LoadInEP(MSC_IN, &mscBufferIn[0], size);
    putstr("\nSend READ(10) : ");
    putdec16(size, 1);
    mscCbwDataTransferLength -= size;
    mscBufferInLength -= size;
    // データ長の矛盾
    if (mscCbwDataTransferLength > 0) {
        if (mscBufferInLength > 0) {
            mscState = MSCST_IN_WAIT;
        } else {
            csw[12] = 1;
            mscState = MSCST_CSW_PREPARE;
        }
    } else {
        if (mscBufferInLength > 0) {
            csw[12] = 1;
            mscState = MSCST_CSW_PREPARE;
        } else {
            mscState = MSCST_CSW_PREPARE;
        }
    }
}

// 未対応OUT命令応答
void mscScsiUnknownOutPrepare(void) {
}

// TEST UNIT READYコマンド応答
void mscScsiTestUnitReadyPrepare(void) {
    mscScsiSenseDataInit();    
    mscState = MSCST_CSW_PREPARE;
}

// 未対応NO DATA命令応答
void mscScsiUnknownNoDataPrepare(void) {
    mscScsiSenseDataInit();
    mscScsiInvalidCommand();
    csw[12] = 1;                        // Command failed
    mscState = MSCST_CSW_PREPARE;
}

// CBW待ち状態の処理
void mscCbwWait(void) {
    if (USBUART_GetEPState(MSC_OUT) & USBUART_OUT_BUFFER_FULL) {
        // パケット長を取得
        mscCbwLength = USBUART_GetEPCount(MSC_OUT);
        
        // CBWデータを読み込む
        USBUART_ReadOutEP(MSC_OUT, &cbw[0], mscCbwLength);

        // CSWのタグを設定する
        csw[4] = cbw[4];
        csw[5] = cbw[5];
        csw[6] = cbw[6];
        csw[7] = cbw[7];
        
        // CSWの初期値を設定する
        csw[12] = 0;

        // CBWデータを表示する
        mscShowCbw();
        
        // データ転送長を保存
        mscCbwDataTransferLength = mscCbwGetValue32(&cbw[8]);
        putstr("\nDataTransferLength=");
        putdec32(mscCbwDataTransferLength, 1);
    
        // データの準備へ分岐
        if (mscCbwIsValid()) {          // コマンドとして受け入れられるか？
            mscState = MSCST_PREPARE;
        }
    }
}

// ホストへのデータ準備
void mscScsiPrepare(void) {
    // コマンドによる処理分岐
    switch (cbw[15]) {
        case SCSI_INQUIRY:
            mscScsiInquiryPrepare();
            break;
        case SCSI_REQUEST_SENSE:
            mscScsiRequestSensePrepare();
            break;
        case SCSI_READ_CAPACITY_10:
            mscScsiReadCapacityPrepare();
            break;
        case SCSI_READ_10:
            mscScsiRead10Prepare();
            break;
        case SCSI_TEST_UNIT_READY:
            mscScsiTestUnitReadyPrepare();
            break;
        default:
            if (mscCbwDataTransferLength == 0) {
                mscScsiUnknownNoDataPrepare();
            } else if (cbw[12]) {
                mscScsiUnknownInPrepare();
            } else {
                mscScsiUnknownOutPrepare();
            }
            break;
    }
}

// ホストへのデータ送信待ち
void mscInWait(void) {
    if (USBUART_GetEPState(MSC_IN) & USBUART_IN_BUFFER_EMPTY) {
        mscState = MSCST_IN_SET;
    }
}

// ホストへのデータ送信
void mscInSet(void) {
    // コマンドによる処理分岐
    switch (cbw[15]) {
        case SCSI_INQUIRY:
        case SCSI_REQUEST_SENSE:
        case SCSI_READ_CAPACITY_10:
            mscScsiBufferInSet();
            break;
        case SCSI_READ_10:
            mscScsiRead10Set();
            break;
        default:
            mscScsiBufferInSet();
            break;
    }
}

// CSWデータの準備
void mscCswPrepare(void) {
    mscPutValue32(&csw[8], mscCbwDataTransferLength);
    mscState = MSCST_CSW_WAIT;
}

// CSWデータの送信
void mscCswWait(void) {
    if (USBUART_GetEPState(MSC_IN) & USBUART_IN_BUFFER_EMPTY) {
        putstr("\nSend CSW : ");
        puthex8(csw[12]);
        // バッファを送信する
        USBUART_LoadInEP(MSC_IN, &csw[0], sizeof csw);
        mscState = MSCST_CBW_WAIT;
    }
}

// MSCデバイスのステートマシン
void mscDispatch(void) {
    switch (mscState) {
        case MSCST_CBW_WAIT:
            mscCbwWait();
            break;
        case MSCST_PREPARE:
            mscScsiPrepare();
            break;
        case MSCST_IN_WAIT:
            mscInWait();
            break;
        case MSCST_IN_SET:
            mscInSet();
            break;
        case MSCST_CSW_PREPARE:
            mscCswPrepare();
            break;
        case MSCST_CSW_WAIT:
            mscCswWait();
            break;
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
        
        while (SW2_Read());                     // SW2が押されるまで接続を待つ

        mscInit();                              // MSCデバイスの初期設定

        for (;;) {
            // 設定が変更されたら、再初期化をおこなう
            if (USBUART_IsConfigurationChanged()) {
                break;
            }
            
            // UARTのエコーバック処理
            echoBackUart();
            
            // MSCデバイスのステートマシン
            mscDispatch();

            // SWをLEDにコピーする処理
            LED4_Write(SW2_Read());
            LED3_Write(SW3_Read());
        }
    }
}

/* [] END OF FILE */
