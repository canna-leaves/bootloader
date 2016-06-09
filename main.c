#include <MSP430F427.h>
#include <string.h>

typedef unsigned char            bool;    ///< ������
typedef unsigned char            uint8;   ///< �޷���8λ���ͱ���
typedef unsigned char            u8;      ///< �޷���8λ���ͱ���
typedef signed char              int8;    ///< �з���8λ���ͱ���
typedef unsigned short           uint16;  ///< �޷���16λ���ͱ���
typedef unsigned short           u16;     ///< �޷���16λ���ͱ���
typedef signed short             int16;   ///< �з���16λ���ͱ���
typedef unsigned long            uint32;  ///< �޷���32λ���ͱ���
typedef unsigned long            u32;     ///< �޷���32λ���ͱ���
typedef signed long              int32;   ///< �з���32λ���ͱ���
typedef unsigned long long       uint64;  ///< �з���64λ���ͱ���
typedef unsigned long long       u64;     ///< �з���64λ���ͱ���
typedef signed long long         int64;   ///< �޷���64λ���ͱ���
typedef float                    fp32;    ///< �����ȸ�����(32λ����)
typedef double                   fp64;    ///< ˫���ȸ�����(64λ����)

enum {
  FALSE, ///< ��
  TRUE,  ///< ��
};

#define   RESETVECTORADDR_APP           0xEFFE   ///< Ӧ�ó���λ������ַ
#define   CSTARTADDR_APP                0x8000
#define   CSTARTADDR_BOOTLOADER         0xF000
#define   FLASH_CODE_SEGMENT_SIZE       512      ///< ��flash������С
#define   UART_BUF_SIZE                 256
#define   UART_RX_TIME_OUT              1000     ///< Լ����10ms
#define   USER_TIME_OUT                 10000    ///< Լ����3s
#define   USER_UPDATE_TIME_OUT          3500     ///< Լ����1s

#define   UPDATE_APP_CMD                "update ready!"

#define   CHECK_SUM_SIZE                2

#define MAX_IMAGE_SZIE            (CSTARTADDR_BOOTLOADER-CSTARTADDR_APP)
#define REGISTER_LEN              65                                      ///< �Ĵ�������
#define PACK_SIZE                 (2*(REGISTER_LEN - 1))                  ///< ��Ч���ݰ���С
#define MAX_PACK_INDEX            (MAX_IMAGE_SZIE/PACK_SIZE)              ///< ��������

enum {
  RTU_READ_REGISTERS  =  3,
  RTU_WRITE_REGISTER  =  6,
  RTU_WRITE_REGISTERS = 16,
};

#pragma   pack(1)
typedef struct Rtu_head_t {
  u8 uid;
  u8 fun;
} Rtu_head_t;

typedef struct Rtu_read_registers_t {
  Rtu_head_t head;
  u16 addr;
  u16 count;
}Rtu_read_registers_t;

typedef struct Rtu_read_registers_ack_t {
  Rtu_head_t head;
  u8 bcount;
}Rtu_read_registers_ack_t;

typedef struct Rtu_write_register_t {
  Rtu_head_t head;
  u16 addr;
  u16 value;
}Rtu_write_register_t;

typedef struct Rtu_write_register_ack_t {
  Rtu_head_t head;
  u16 addr;
  u16 value;
}Rtu_write_register_ack_t;

typedef struct Rtu_write_registers_t {
  Rtu_head_t head;
  u16 addr;
  u16 count;
  u8  bcount;
}Rtu_write_registers_t;

typedef struct Rtu_write_registers_ack_t {
  Rtu_head_t head;
  u16 addr;
  u16 count;
}Rtu_write_registers_ack_t;
#pragma   pack()

/**
* @brief ��������ṹ
*/
typedef struct UpdateInfo {
  u8 uid;            ///< �ӻ���
  u16 pack_index;    ///< ��ǰ����������
  u32 file_size;     ///< �����ļ���С
  u32 file_sum;      ///< �����ļ�У���
  u8 first_pack;
  u8 last_pack;      ///< 
}UpdateInfo;

u8 flash_tmp[FLASH_CODE_SEGMENT_SIZE];   ///< flash�ײ㻺����
u8 uart_buf[UART_BUF_SIZE];
UpdateInfo update_info = {.first_pack=FALSE, .last_pack=FALSE};
u16 rx_index      = 0;
u16 rx_idle_count = 0;
u32 usr_idle_count = 0;

void board_init();
void update_task();
void boot_app();
u8 verify_image();
void uart_send(u8 *buf, u16 len);
u8 modbus_handler(u8 *buf, u16 len);
u16 crc16(u8 *buf, u16 len);
u8 flash_write(u8 *addr, u32 offset, u8 *value, u32 len, u16 seg_size);
u16 swap_u16(u16 value);
u16 u8_to_u16_big(u8 *value);
u32 filesum32(u8 *addr, u16 len);
u32 u8_to_u32_big(u8 *value);

int main() {
  board_init();
  uart_send(UPDATE_APP_CMD, sizeof(UPDATE_APP_CMD));
  while(1) {
    update_task();
  }
}

void board_init() {
  WDTCTL = WDTPW + WDTHOLD;
  _DINT();
#if 0
  SCFI0 |= FN_3;                           
  SCFQCTL = 121;                          
  FLL_CTL0 = DCOPLUS + XCAP18PF;         
  __delay_cycles(375000);    
  
  UTCTL0 = SSEL0;                       // ACLK = 32768
  UBR00 = 0x03;                         // 9600 @ 32768
  UBR10 = 0x00;
  UMCTL0 = 0x4A;
  UCTL0 = CHAR;                                               // 8 ������λ
  ME1 |= UTXE0 + URXE0;                                       // ������/����
  IE1 |= URXIE0;                                              // ��������ж�
  P2SEL |= (BIT4 + BIT5);     // ������Ӧ��IO��
  UCTL0 &= ~ SWRST;
#endif
  
  SCFI0 |= FN_2;                           
  SCFQCTL = 31;                          
  FLL_CTL0 = DCOPLUS + XCAP18PF;         
  __delay_cycles(375000);    
  
  UBR00  = 0xDA; 
  UBR10  = 0x00;
  UMCTL0 = 0x90;  //9600
  U0CTL &=~ PENA;
  U0CTL  |= CHAR; 
  //�˿�����
  UTCTL0 = SSEL1;//smclk
  /* USART0 TXD/RXD */
  ME1   |= UTXE0 + URXE0;                // ������/����
  P2DIR |= BIT4;
  P2DIR &= ~BIT5;
  P2SEL |= (BIT4 + BIT5);        // ������Ӧ��IO��
  UCTL0 &= ~ SWRST; 
  _EINT();
}
void update_task() {
  u8 rx_tmp;
  if(IFG1 & URXIFG0) {
    rx_tmp = RXBUF0;
    if(rx_index < UART_BUF_SIZE) {
      uart_buf[rx_index++] = rx_tmp;
    }
    rx_idle_count = 0;
    usr_idle_count = 0;
  } else {
    ++rx_idle_count;
    if(rx_idle_count > UART_RX_TIME_OUT) {
      if(rx_index > sizeof(Rtu_head_t)) {
        modbus_handler(uart_buf, rx_index);
      }
      rx_idle_count = 0;
      rx_index = 0;
      //uart_send(uart_buf, 5);
    }
    ++usr_idle_count;
    if(0 == usr_idle_count%USER_UPDATE_TIME_OUT) {
      uart_send(UPDATE_APP_CMD, sizeof(UPDATE_APP_CMD));
    }
    if(usr_idle_count > USER_TIME_OUT) {
      usr_idle_count = 0;
      if(TRUE == verify_image()) {
        boot_app();
      }
    }
  }
}


u8 modbus_handler(u8 *buf, u16 len) {
  Rtu_head_t *head = (Rtu_head_t *)buf;
  if(head->uid < 1 || head->uid > 0x99) return FALSE;
  if(RTU_READ_REGISTERS == head->fun && len >= sizeof(Rtu_read_registers_t) + CHECK_SUM_SIZE) {
    u16 crc = crc16(buf, sizeof(Rtu_read_registers_t) + CHECK_SUM_SIZE);
    if(crc != 0) {
      return FALSE;
    }
    Rtu_read_registers_t *req = (Rtu_read_registers_t *)buf;
    u16 count = swap_u16(req->count);
    //u16 addr  = req->addr;
    if(count > 125) {
      count = 125;
    }
    Rtu_read_registers_ack_t *ack = (Rtu_read_registers_ack_t *)buf;
    ack->bcount = count * 2;
    u8 *data = buf + sizeof(Rtu_read_registers_ack_t);
    memset(data, 0xFF, ack->bcount);
    data += ack->bcount;
    crc = crc16(buf, sizeof(Rtu_read_registers_ack_t) + ack->bcount);
    *data++ = crc&0xFF;*data = crc>>8;
    uart_send(buf, sizeof(Rtu_read_registers_ack_t) + ack->bcount + CHECK_SUM_SIZE);
    // printf_hex(buf, sizeof(Rtu_read_registers_ack_t) + ack->bcount + CHECK_SUM_SIZE);
    //printf("\n"); 
  } else if(RTU_WRITE_REGISTER == head->fun && len >= sizeof(Rtu_write_register_t) + CHECK_SUM_SIZE) {
    Rtu_write_register_t *req = (Rtu_write_register_t *)buf;
    u16 crc = crc16(buf, sizeof(Rtu_write_register_t) + CHECK_SUM_SIZE);
    if(crc != 0) {
      return FALSE;
    }
    u16 addr = swap_u16(req->addr);
    u16 value = swap_u16(req->value);
    //Rtu_write_register_t *req = (Rtu_write_register_t *)buf;
    //req->value;
    uart_send(buf, sizeof(Rtu_write_register_t) + CHECK_SUM_SIZE);
    if(1000 == addr && 1 == value) {
      __delay_cycles(10000);
      if(TRUE == verify_image()) {
        boot_app();
      }
    }
    //printf_hex(buf, sizeof(Rtu_write_register_t) + CHECK_SUM_SIZE);
    //printf("\n");
  } else if(RTU_WRITE_REGISTERS == head->fun && len >= sizeof(Rtu_write_registers_t) + CHECK_SUM_SIZE) {
    Rtu_write_registers_t *req = (Rtu_write_registers_t *)buf;
    u16 count = swap_u16(req->count);
    u16 addr = swap_u16(req->addr);
    if(count < 1) {
      return FALSE;
    }
    if(count > 125) {
      count = 125;
    }
    if((count*2 != req->bcount) || (len < req->bcount + sizeof(Rtu_write_registers_t) + CHECK_SUM_SIZE)) {
      return FALSE;
    }
    u16 crc = crc16(buf, req->bcount + sizeof(Rtu_write_registers_t) + CHECK_SUM_SIZE);
    if(crc != 0) {
      return FALSE;
    }
    u8 *data = buf + sizeof(Rtu_write_registers_t);
    //Rtu_write_registers_ack_t *ack = (Rtu_write_registers_ack_t *)buf;
    if(1000 == addr && count >= REGISTER_LEN) {
      u16 pack_index = u8_to_u16_big(data);
      if(0 == pack_index) {
        update_info.pack_index = 0;
        update_info.last_pack = FALSE;
        update_info.uid = req->head.uid;
        data += sizeof(u16);
        update_info.file_size = u8_to_u32_big(data);
        data += sizeof(u32);
        update_info.file_sum = u8_to_u32_big(data);
        if(update_info.file_size > MAX_IMAGE_SZIE) {
          return FALSE;
        }
        update_info.first_pack = TRUE;
      } else if(pack_index <= MAX_PACK_INDEX) {
        if(update_info.uid != req->head.uid || pack_index != update_info.pack_index + 1 || FALSE == update_info.first_pack) {
          return FALSE;
        }
        __disable_interrupt();
        flash_write((u8 *)CSTARTADDR_APP, PACK_SIZE * (pack_index - 1), data + sizeof(u16), PACK_SIZE, FLASH_CODE_SEGMENT_SIZE);
        __enable_interrupt();
        update_info.pack_index = pack_index;
        
        if(pack_index * PACK_SIZE >= update_info.file_size) {
          __disable_interrupt();
          u32 file_sum = filesum32((u8 *)CSTARTADDR_APP, update_info.file_size);
          __enable_interrupt();
          if(file_sum != update_info.file_sum) {
            return FALSE;
          }
          //boot_app();
          update_info.last_pack = TRUE;
        }
      } else {
        return FALSE;
      }
    }
    data = buf + sizeof(Rtu_write_registers_ack_t);
    crc = crc16(buf, sizeof(Rtu_write_registers_ack_t));
    *data++ = crc&0xFF;*data = crc>>8;
    uart_send(buf, sizeof(Rtu_write_registers_ack_t) + CHECK_SUM_SIZE);
    //printf_hex(buf, sizeof(Rtu_write_registers_ack_t) + CHECK_SUM_SIZE);
    //printf("\n");
    if(TRUE == update_info.last_pack) {
      update_info.first_pack = FALSE;
      update_info.last_pack = FALSE;
      __delay_cycles(10000);
      if(TRUE == verify_image()) {
        boot_app();
      }
    }
  } else {
    return FALSE;
  }
  return TRUE;
}

u16 swap_u16(u16 value) {
  return (value>>8) | (value<<8);
}

u32 filesum32(u8 *addr, u16 len) {
  u16 i;
  u32 sum = 0;
  for(i=0;i<len;++i) {
    sum += *addr++;
  }
  return sum;
}

u32 u8_to_u32_big(u8 *value) {
  return value[3] | ((u16)value[2]<<8) | ((u32)value[1]<<16) | ((u32)value[0]<<24);
}

void boot_app() {
  asm(" mov &0xEFFE, PC;");
}
u8 verify_image() {
  u8 value1;
  u8 value2;
  while(FCTL3 & BUSY);
  value1 = *(u8 *)RESETVECTORADDR_APP;
  value2 = *(u8 *)(RESETVECTORADDR_APP + 1);
  if((0xFF == value1) && (0xFF == value2)) {
    return FALSE;
  }
  return TRUE;
}
void uart_send(u8 *buf, u16 len) {
  u16 i;
  for(i=0;i<len;++i) {
    while(!(IFG1&UTXIFG0)); //query tx ready?
    TXBUF0 = *buf++;
  }
}

u16 u8_to_u16_big(u8 *value) {
  return value[1] | (u16)(value[0]<<8);
}

u16 crc16(u8 *buf, u16 len) {
  u16 b = 0xA001;
  u16 a = 0xFFFF;
  int i, j;
  u8 value;
  u8 last;
  for(i=0;i<len;++i){
    value = buf[i];
    a ^= value;
    for(j=0;j<8;++j){
      last = a%2;
      a >>= 1;
      if(1==last){
        a ^= b;
      }	
    }
  }
  return a;
}

#if 0
void flash_erase_one_seg(u8 *addr) {
  while(BUSY == (FCTL3&BUSY));
  FCTL3 = FWKEY;                            // Clear Lock bit
  FCTL1 = FWKEY+ERASE;                      // Set Erase bit
  *(u16 *)addr = 0;                         // Dummy write to erase Flash seg
  while(BUSY == (FCTL3&BUSY));
  FCTL3 = FWKEY+LOCK;                       // Set LOCK bit
}
#endif

/**
* @brief дһ���������ݵ�flash
* @param[in] addr ������ʼ��ַ
* @param[in] value ����
* @param[in] seg_szie ������С
* @return �޷���ֵ
*/
void flash_write_one_seg(u8 *addr, u8 *value, u16 seg_szie) {
  u16 i;
  while(BUSY == (FCTL3&BUSY));
  FCTL3 = FWKEY;                            // Clear Lock bit
  FCTL1 = FWKEY+ERASE;                      // Set Erase bit
  *(u16 *)addr = 0;                         // Dummy write to erase Flash seg
  while(BUSY == (FCTL3&BUSY));
  FCTL1 = FWKEY+WRT;                        // Set WRT bit for write operation
  
  for (i = 0; i < seg_szie; i++)
  {
    *addr++ = *value++;                     // Write value to flash
    while(!(WAIT & FCTL3));
  }
  FCTL1 = FWKEY;                            // Clear WRT bit
  while(BUSY == (FCTL3&BUSY));
  FCTL3 = FWKEY+LOCK;                       // Set LOCK bit
}

#define flash_ramcpy memcpy  ///< �ض���copy����
#if 0
void flash_ramcpy(u8 *dst, u8 *src, u32 len) {
  if(0 == len) {
    return;
  }
  u32 i;
  for(i = 0;i < len;++i) {
    *dst++ = *src++;
  }
}
#endif

/**
* @brief дflash(������ֹ�ж�)
* @param[in] addr ������ʼ��ַ
* @param[in] offset ��ַƫ��
* @param[in] value ����
* @param[in] len ����
* @param[in] seg_size ������С
* @retval TRUE �ɹ�
* @retval FALSE ʧ��
*/
u8 flash_write(u8 *addr, u32 offset, u8 *value, u32 len, u16 seg_size) {
  if(0 == len) {
    return FALSE;
  }
  u16 i;
  //__disable_interrupt();                    // 5xx Workaround: Disable global
  // interrupt while erasing. Re-Enable
  // GIE if needed
  //u8 tmp[FLASH_SEGMENT_SIZE];
  u8 *flash_addr = addr + (offset/seg_size*seg_size);
  u32 count_left = offset%seg_size;
  u32 count_right = (offset + len)%seg_size;
  
  u32 seg_num = ((offset + len)/seg_size - offset/seg_size);
  
  flash_ramcpy(flash_tmp, flash_addr, seg_size);
  if(seg_size - count_left > len) {
    flash_ramcpy(flash_tmp + count_left, value, len);
    value += len;
  } else {
    flash_ramcpy(flash_tmp + count_left, value, seg_size - count_left);
    value += seg_size - count_left;
  }
  flash_write_one_seg(flash_addr, flash_tmp, seg_size);
  flash_addr += seg_size;
  
  for(i = 1;i < seg_num;++i) {
    flash_write_one_seg(flash_addr, value, seg_size);
    flash_addr += seg_size;
    value += seg_size;
  }
  
  if(0 != count_right && seg_size - count_left < len) {
    flash_ramcpy(flash_tmp, flash_addr, seg_size);
    flash_ramcpy(flash_tmp, value, count_right);
    flash_write_one_seg(flash_addr, flash_tmp, seg_size);
    flash_addr += seg_size;
    value += count_right;
  }
  //__enable_interrupt();
  return TRUE;
}
#if 1
//******************************************************************************
// ����: �ж������б�
#pragma vector=0
__interrupt void intec_0(void)
{
  asm(" br &0xEFE0;");
}

#pragma vector=2
__interrupt void intec_1(void)
{
  asm(" br &0xEFE2;");
}

#pragma vector=4
__interrupt void intec_2(void)
{
  asm(" br &0xEFE4;");
}

#pragma vector=6
__interrupt void intec_3(void)
{
  asm(" br &0xEFE6;");
}


#pragma vector=8
__interrupt void intec_4(void)
{
  asm(" br &0xEFE8;");
  
}

#pragma vector=10
__interrupt void intec_5(void)
{
  asm(" br &0xEFEA;");  
}

#pragma vector=12
__interrupt void intec_6(void)
{
  asm(" br &0xEFEC;");
}

#pragma vector=14
__interrupt void intec_7(void)
{
  asm(" br &0xEFEE;");
}

#pragma vector=16
__interrupt void intec_8(void)
{
  asm(" br &0xEFF0;");
}

#pragma vector=18 
__interrupt void intec_9(void) // Ӧ�ó���Ĵ����ж�0���ж�������ַ
{
  asm(" br &0xEFF2;");
}

#pragma vector=20
__interrupt void intec_10(void)
{
  asm(" br &0xEFF4;");
}

#pragma vector=22
__interrupt void intec_11(void)
{
  asm(" br &0xEFF6;");
}

#pragma vector=24
__interrupt void intec_12(void)
{
  asm(" br &0xEFF8;");
}

#pragma vector=26
__interrupt void intec_13(void)
{
  asm(" br &0xEFFA;");
}

#pragma vector=28
__interrupt void intec_14(void)
{
  asm(" br &0xEFFC;");
}
#endif