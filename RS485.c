#include "RS485.h"

#define READ_COILS                  0x01
#define READ_DISCRETE_INPUTS        0x02
#define READ_HOLDING_REGISTERS      0x03
#define READ_INPUT_REGISTERS        0x04
#define WRITE_SINGLE_COIL           0x05
#define WRITE_SINGLE_REGISTER       0x06
#define READ_EXCEPTION_STATUS       0x07
#define READ_WRITE_MULTIPLE_REGISTERS    0x17


#define ADDRESS                     0
#define FUNCTION                    1

#define STARTING_ADDRESS_HI         2
#define STARTING_ADDRESS_LO         3
#define QUANTITY_OF_COILS_HI        4
#define QUANTITY_OF_COILS_LO        5

#define QUANTITY_OF_REGISTERS_HI    4
#define QUANTITY_OF_REGISTERS_LO    5

#define WRITE_STARTING_ADDRESS_HI   6
#define WRITE_STARTING_ADDRESS_LO   7

#define QUANTITY_TO_WRITE_REGISTERS_HI  8
#define QUANTITY_TO_WRITE_REGISTERS_LO  9

#define WRITE_BYTE_COUNT            10

#define U_CALIBR_VALUE_HI           11
#define U_CALIBR_VALUE_LO           12

#define I_CALIBR_VALUE_HI           13
#define I_CALIBR_VALUE_LO           14

#define U_CALIBR_VALUE_SEND_HI      3
#define U_CALIBR_VALUE_SEND_LO      4
#define I_CALIBR_VALUE_SEND_HI      5
#define I_CALIBR_VALUE_SEND_LO      6

#define COIL_VALUE_HI               4
#define COIL_VALUE_LO               5

#define BYTE_COUNT                  2
#define COILS_STATUS                3

#define ERROR_CODE                  1
#define EXCEPTION_CODE              2

#define OFF_500V_COIL               0
#define ON_500V_PLUS_COIL           1
#define ON_500V_MINUS_COIL          2
#define ON_GROUND_COIL              3

void send_Coils(uint8_t receive[EUSART1_RX_BUFFER_SIZE]);
void send_Input_Registers(uint8_t receive[EUSART1_RX_BUFFER_SIZE]);
void send_Write_Coil(uint8_t receive[EUSART1_RX_BUFFER_SIZE]);
void write_Calibr_Coefs(uint8_t receive[EUSART1_RX_BUFFER_SIZE]);

void send_Error_Code(uint8_t receive[EUSART1_RX_BUFFER_SIZE], uint8_t exception);

//answer_frame answer;

/* Адреса COILов
 * 
 * 1 ВЫКЛ 500В
 * 2 ВКЛ +500В
 * 3 ВКЛ -500В
 * 4 ЗЕМЛЯ
 */

/* Адреса Input Registers 
 * 
 * 1 R - Resistance     2000
 * 2 U - Voltage        2002
 * 3 I - Current        2004
 */

/* Адреса Регистров коэффициентов 
 * 
 * 20 R - Resistance
 * 21 I - Current
 * 22 U - Voltage
 */

extern bool OFF_500V;
extern bool ON_500V_Plus;
extern bool ON_500V_Minus;
extern bool Ground;

static measures response_measure;

// | сюда после срабатывания таймера, то есть после свободности в линии после принятия кадра
// V размер принятого кадра меньше размера буфера приемника УАРТ
void recieve_frame(uint8_t size)
{
    uint8_t request[EUSART1_RX_BUFFER_SIZE], i;
    
    for (i = 0; i < size; i++)
    {
        request[i] = EUSART1_Read();
    }
    
    uint16_t receive_CRC = 0;
    
    receive_CRC = request[size-1];
    receive_CRC <<= 8;
    receive_CRC |= request[size-2];
    
    if (receive_CRC != CRC16(request, size-2))
        return;
    
    if (request[ADDRESS] != get_addr())
        return;
    
    switch (request[FUNCTION])
    {
        case READ_COILS:
        {
            send_Coils(request);
            break;
        }
        case READ_DISCRETE_INPUTS:
        {
            //ФУНКЦИЯ НЕ ПОДДЕРЖИВАЕТСЯ EXCEPTION_CODE == 1
            send_Error_Code(request, 1);
            break;
        }
        case READ_HOLDING_REGISTERS:
        {
            //ФУНКЦИЯ НЕ ПОДДЕРЖИВАЕТСЯ EXCEPTION_CODE == 1
            send_Error_Code(request, 1);
            break;
        }
        case READ_INPUT_REGISTERS:
        {
            send_Input_Registers(request);
            break;
        }
        case WRITE_SINGLE_COIL:
        {
            send_Write_Coil(request);
            break;
        }
        case READ_EXCEPTION_STATUS:
        {
            //ФУНКЦИЯ НЕ ПОДДЕРЖИВАЕТСЯ EXCEPTION_CODE == 1
            send_Error_Code(request, 1);
            break;
        }
        case READ_WRITE_MULTIPLE_REGISTERS:
        {
            //send_Error_Code(request, 1);
            write_Calibr_Coefs(request);
            break;
        }
        default:
        {
            //ФУНКЦИЯ НЕ ПОДДЕРЖИВАЕТСЯ EXCEPTION_CODE == 1
            send_Error_Code(request, 1);
            break;
        }
    }
}

void send_Coils(uint8_t *request)
{
    //проверим адекватность запроса
    if (request[STARTING_ADDRESS_LO] > 3 ||
        request[QUANTITY_OF_COILS_LO] < 1 ||
        request[QUANTITY_OF_COILS_LO] > 4 ||
        request[QUANTITY_OF_COILS_LO] > (4-request[STARTING_ADDRESS_LO]) ||     
        request[STARTING_ADDRESS_HI] != 0 ||
        request[QUANTITY_OF_COILS_HI] != 0)
    {
        //EXCEPTION_CODE == 2
        send_Error_Code(request, 2);
        return;
    }
        
    uint8_t response[6];
    
    response[ADDRESS] = get_addr();
    response[FUNCTION] = READ_COILS;
    response[BYTE_COUNT] = 0x01;
    response[COILS_STATUS] = 0x00 | (Ground<<3) | (ON_500V_Minus<<2) | (ON_500V_Plus<<1) | (OFF_500V<<0);
    response[COILS_STATUS] >>= request[STARTING_ADDRESS_LO];
    response[COILS_STATUS] <<= (8 - request[QUANTITY_OF_COILS_LO]);
    response[COILS_STATUS] >>= (8 - request[QUANTITY_OF_COILS_LO]);
    
    //рассчитать СRC
    uint16_t tempCRC;
    
    tempCRC = CRC16(response, sizeof(response)-2);
    
    response[4] = (uint8_t)(tempCRC);
    response[5] = (uint8_t)(tempCRC >> 8);
    
    send(response, sizeof(response));
}

void send_Input_Registers(uint8_t *request)
{
    if (request[STARTING_ADDRESS_LO] < 0xD0 ||
        request[STARTING_ADDRESS_LO] > 0xD5 ||
        request[STARTING_ADDRESS_HI] != 0x07 ||
        request[QUANTITY_OF_REGISTERS_LO] < 1 ||
        request[QUANTITY_OF_REGISTERS_LO] > 6 ||
        request[QUANTITY_OF_REGISTERS_HI] != 0)
    {
        //EXCEPTION_CODE == 2
        send_Error_Code(request, 2);
        return;
    }
    
    request[STARTING_ADDRESS_HI] = request[STARTING_ADDRESS_HI] - 0x07;
    request[STARTING_ADDRESS_LO] = request[STARTING_ADDRESS_LO] - 0xD0;
    
    uint8_t response[17], i, j;
    union FloatChar
    {
        float fl;
        uint8_t ch[4];
    };
    
    union FloatChar resistance, voltage, current;
    
    resistance.fl = response_measure.resistance;
    voltage.fl = response_measure.voltage;
    current.fl = response_measure.current;
        
    response[ADDRESS] = get_addr();
    response[FUNCTION] = READ_INPUT_REGISTERS;
    response[BYTE_COUNT] = request[QUANTITY_OF_REGISTERS_LO] * 2;
    response[3] = resistance.ch[0];
    response[4] = resistance.ch[1];
    response[5] = resistance.ch[2];
    response[6] = resistance.ch[3];
    response[7] = voltage.ch[0];
    response[8] = voltage.ch[1];
    response[9] = voltage.ch[2];
    response[10] = voltage.ch[3];
    response[11] = current.ch[0];
    response[12] = current.ch[1];
    response[13] = current.ch[2];
    response[14] = current.ch[3];
    
    j = 2;
    for(i = 3 + request[STARTING_ADDRESS_LO]*2; i < 3 + request[STARTING_ADDRESS_LO]*2 + request[QUANTITY_OF_REGISTERS_LO]*2; i++)
    {
        j++;
        response[j] = response[i];
    }
    
    //3 + request[STARTING_ADDRESS_LO]*2; // откуда
    //3 + request[STARTING_ADDRESS_LO]*2 + request[QUANTITY_OF_REGISTERS_LO]; // докуда
    
    
    //рассчитать СRC
    uint16_t tempCRC;
    
    tempCRC = CRC16(response, j+1);
    
    response[j+1] = (uint8_t)(tempCRC);
    response[j+2] = (uint8_t)(tempCRC >> 8);
    
    send(response, j+3);
}



/*void send_Input_Registers(uint8_t *request)
{
    if (request[STARTING_ADDRESS_LO] > 2 ||
        request[QUANTITY_OF_REGISTERS_LO] < 1 ||
        request[QUANTITY_OF_REGISTERS_LO] > 3 ||
        request[QUANTITY_OF_REGISTERS_LO] > (3-request[STARTING_ADDRESS_LO]) ||
        request[STARTING_ADDRESS_HI] != 0 ||
        request[QUANTITY_OF_REGISTERS_HI] != 0)
    {
        //EXCEPTION_CODE == 2
        send_Error_Code(request, 2);
        return;
    }
    
    
    uint8_t response[11], i;
    uint16_t voltage, current, res;
    uint64_t temp = 0;
    
    voltage = (uint16_t)(response_measure.voltage * 10);
    current = (uint16_t)(response_measure.current * 10);
    res = (uint16_t)(response_measure.resistance * 100);
    
    temp |= voltage;
    temp <<= 16;
    temp |= current;
    temp <<= 16;
    temp |= res;
    
    response[ADDRESS] = get_addr();
    response[FUNCTION] = READ_INPUT_REGISTERS;
    response[BYTE_COUNT] = request[QUANTITY_OF_REGISTERS_LO] * 2;
    temp >>= request[STARTING_ADDRESS_LO] * 16;
    temp <<= (64 - request[QUANTITY_OF_REGISTERS_LO]*16);
    temp >>= (64 - request[QUANTITY_OF_REGISTERS_LO]*16);
    for (i = 0; i < request[QUANTITY_OF_REGISTERS_LO]; i++)
    {
        response[BYTE_COUNT+1+2*i] = (uint8_t)(temp >> (16*i+8));          //HI
        response[BYTE_COUNT+2+2*i] = (uint8_t)(temp >> (16*i));          //LO
    }
    
    //рассчитать СRC
    uint16_t tempCRC;
    
    tempCRC = CRC16(response, 3+response[BYTE_COUNT]);
    
    response[5+response[BYTE_COUNT]-2] = (uint8_t)(tempCRC);
    response[5+response[BYTE_COUNT]-1] = (uint8_t)(tempCRC >> 8);
    
    send(response, 5+response[BYTE_COUNT]);
}
*/
void send_Write_Coil(uint8_t *request)
{
    //проверим адекватность запроса
    if (request[STARTING_ADDRESS_LO] > 3 ||
        request[STARTING_ADDRESS_HI] != 0 ||
        request[COIL_VALUE_HI] != 0xFF ||
        request[COIL_VALUE_LO] != 0x00)
    {
        //EXCEPTION_CODE == 2
        send_Error_Code(request, 2);
        return;
    }
    switch(request[STARTING_ADDRESS_LO])
    {
        case OFF_500V_COIL:
        {
            TurnOFF_500V();
            break;
        }
        case ON_500V_PLUS_COIL:
        {
            TurnON_500V_Plus();
            break;
        }
        case ON_500V_MINUS_COIL:
        {
            TurnON_500V_Minus();
            break;
        }
        case ON_GROUND_COIL:
        {
            TurnON_GND();
            break;
        }
        default:
        {
            TurnOFF_500V();
            break;
        }
    }
    
    uint16_t tempCRC;
    
    tempCRC = CRC16(request, 8-2);
    
    request[6] = (uint8_t)(tempCRC);
    request[7] = (uint8_t)(tempCRC >> 8);    
    
    send(request, 8);
}

void write_Calibr_Coefs(uint8_t *request)
{
    //проверим адекватность запроса
    if (request[STARTING_ADDRESS_LO] != 0xE8 ||
        request[STARTING_ADDRESS_HI] != 0x03 ||
        request[QUANTITY_OF_REGISTERS_LO] != 12 ||
        request[QUANTITY_OF_REGISTERS_HI] != 0 ||
        request[WRITE_STARTING_ADDRESS_LO] != 0xE8 ||
        request[WRITE_STARTING_ADDRESS_HI] != 0x03 ||
        request[QUANTITY_TO_WRITE_REGISTERS_LO] != 12 ||
        request[QUANTITY_TO_WRITE_REGISTERS_HI] != 0 ||
        request[WRITE_BYTE_COUNT] != 24)
    {
        //EXCEPTION_CODE == 2
        send_Error_Code(request, 2);
        return;
    }
    
    
    union FloatChar
    {
        float fl;
        uint8_t ch[4];
    };
    union FloatChar U_coef, U_bias, I_above_100_coef, I_above_100_bias, I_below_100_coef, R_bias;
    
    U_coef.ch[0] = request[11];
    U_coef.ch[1] = request[12];
    U_coef.ch[2] = request[13];
    U_coef.ch[3] = request[14];
    U_bias.ch[0] = request[15];
    U_bias.ch[1] = request[16];
    U_bias.ch[2] = request[17];
    U_bias.ch[3] = request[18];
    I_above_100_coef.ch[0] = request[19];
    I_above_100_coef.ch[1] = request[20];
    I_above_100_coef.ch[2] = request[21];
    I_above_100_coef.ch[3] = request[22];
    I_above_100_bias.ch[0] = request[23];
    I_above_100_bias.ch[1] = request[24];
    I_above_100_bias.ch[2] = request[25];
    I_above_100_bias.ch[3] = request[26];
    I_below_100_coef.ch[0] = request[27];
    I_below_100_coef.ch[1] = request[28];
    I_below_100_coef.ch[2] = request[29];
    I_below_100_coef.ch[3] = request[30];
    R_bias.ch[0] = request[31];
    R_bias.ch[1] = request[32];
    R_bias.ch[2] = request[33];
    R_bias.ch[3] = request[34];
    
    eeprom_write_object(0x01, &U_coef.fl, sizeof(float));
    eeprom_write_object(0x05, &U_bias.fl, sizeof(float));
    eeprom_write_object(0x09, &I_above_100_coef.fl, sizeof(float));
    eeprom_write_object(0x0D, &I_above_100_bias.fl, sizeof(float));
    eeprom_write_object(0x11, &I_below_100_coef.fl, sizeof(float));
    eeprom_write_object(0x15, &R_bias.fl, sizeof(float));
    
    
    eeprom_read_object(0x01, &U_coef.fl, sizeof(float));
    eeprom_read_object(0x05, &U_bias.fl, sizeof(float));
    eeprom_read_object(0x09, &I_above_100_coef.fl, sizeof(float));
    eeprom_read_object(0x0D, &I_above_100_bias.fl, sizeof(float));
    eeprom_read_object(0x11, &I_below_100_coef.fl, sizeof(float));
    eeprom_read_object(0x15, &R_bias.fl, sizeof(float));
    
    //uint8_t response[13];
    
    request[ADDRESS] = get_addr();
    request[FUNCTION] = READ_WRITE_MULTIPLE_REGISTERS;
    request[BYTE_COUNT] = 24;
    request[3] = U_coef.ch[0];
    request[4] = U_coef.ch[1];
    request[5] = U_coef.ch[2];
    request[6] = U_coef.ch[3];
    request[7] = U_bias.ch[0];
    request[8] = U_bias.ch[1];
    request[9] = U_bias.ch[2];
    request[10] = U_bias.ch[3];
    request[11] = I_above_100_coef.ch[0];
    request[12] = I_above_100_coef.ch[1];
    request[13] = I_above_100_coef.ch[2];
    request[14] = I_above_100_coef.ch[3];
    request[15] = I_above_100_bias.ch[0];
    request[16] = I_above_100_bias.ch[1];
    request[17] = I_above_100_bias.ch[2];
    request[18] = I_above_100_bias.ch[3];
    request[19] = I_below_100_coef.ch[0];
    request[20] = I_below_100_coef.ch[1];
    request[21] = I_below_100_coef.ch[2];
    request[22] = I_below_100_coef.ch[3];
    request[23] = R_bias.ch[0];
    request[24] = R_bias.ch[1];
    request[25] = R_bias.ch[2];
    request[26] = R_bias.ch[3];
    
    //рассчитать СRC
    uint16_t tempCRC;
    
    tempCRC = CRC16(request, 27);
    
    request[27] = (uint8_t)(tempCRC);
    request[28] = (uint8_t)(tempCRC >> 8);
    
    send(request, 29);
}


/*void write_Calibr_Coefs(uint8_t *request)
{
    //проверим адекватность запроса
    if (request[STARTING_ADDRESS_LO] != 0 ||
        request[STARTING_ADDRESS_HI] != 0 ||
        request[QUANTITY_OF_REGISTERS_LO] != 2 ||
        request[QUANTITY_OF_REGISTERS_HI] != 0 ||
        request[WRITE_STARTING_ADDRESS_LO] != 0 ||
        request[WRITE_STARTING_ADDRESS_HI] != 0 ||
        request[QUANTITY_TO_WRITE_REGISTERS_LO] != 2 ||
        request[QUANTITY_TO_WRITE_REGISTERS_HI] != 0 ||
        request[WRITE_BYTE_COUNT] != 4)
    {
        //EXCEPTION_CODE == 2
        send_Error_Code(request, 2);
        return;
    }
        
    float I_temp, U_temp;
    uint16_t U_temp16, I_temp16;
    
    U_temp16 = request[U_CALIBR_VALUE_HI];
    U_temp16 <<= 8;
    U_temp16 |= request[U_CALIBR_VALUE_LO];
    
    I_temp16 = request[I_CALIBR_VALUE_HI];
    I_temp16 <<= 8;
    I_temp16 |= request[I_CALIBR_VALUE_LO];
    
    U_temp = (float) (U_temp16);
    I_temp = (float) (I_temp16);
    U_temp /= 1000;
    I_temp /= 1000;
    eeprom_write_object(0x01, &U_temp, sizeof(U_temp));
    eeprom_write_object(0x05, &I_temp, sizeof(I_temp));
    
    
    eeprom_read_object(0x01, &U_temp, sizeof(float));
    eeprom_read_object(0x05, &I_temp, sizeof(float));
    
    U_temp16 = (uint16_t) (U_temp * 1000);
    I_temp16 = (uint16_t) (I_temp * 1000);
    
    uint8_t response[9];
    
    response[ADDRESS] = get_addr();
    response[FUNCTION] = READ_WRITE_MULTIPLE_REGISTERS;
    response[BYTE_COUNT] = 4;
    response[U_CALIBR_VALUE_SEND_HI] = (uint8_t)(U_temp16 >> 8);
    response[U_CALIBR_VALUE_SEND_LO] = (uint8_t)(U_temp16);
    response[I_CALIBR_VALUE_SEND_HI] = (uint8_t)(I_temp16 >> 8);
    response[I_CALIBR_VALUE_SEND_LO] = (uint8_t)(I_temp16);
    
    //рассчитать СRC
    uint16_t tempCRC;
    
    tempCRC = CRC16(response, 7);
    
    response[7] = (uint8_t)(tempCRC);
    response[8] = (uint8_t)(tempCRC >> 8);
    
    send(response, 9);
}
*/

void send_Error_Code(uint8_t *request, uint8_t exception)
{
    uint8_t response[5];
    
    response[ADDRESS] = get_addr();
    response[ERROR_CODE] = request[FUNCTION] + 0x80;
    response[EXCEPTION_CODE] = exception;
    
    //рассчитать CRC
    uint16_t tempCRC;
    
    tempCRC = CRC16(request, sizeof(response)-2);
    
    request[3] = (uint8_t)(tempCRC >> 8);
    request[4] = (uint8_t)(tempCRC);
    
    send(response, sizeof(response));
}

void send(uint8_t *chptr, uint8_t size)
{
    uint8_t i;
    
    TX_nRC_SetHigh();
    for (i = 0; i < size; i++)
        EUSART1_Write(*chptr++);
}

void send_done(void)
{
    TX_nRC_SetLow();
}

void save_measure(measures measure)
{
    // Disable the Global Interrupts
    INTERRUPT_GlobalInterruptDisable();
    
    response_measure = measure;
    
    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();
    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();
}
