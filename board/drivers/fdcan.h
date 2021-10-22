// IRQs: FDCAN1_IT0, FDCAN1_IT1
//       FDCAN2_IT0, FDCAN2_IT1
//       FDCAN3_IT0, FDCAN3_IT1

#define BUS_OFF_FAIL_LIMIT 2U
uint8_t bus_off_err[] = {0U, 0U, 0U};

// TEMPORARY, not yet sure that I need it, might be just easier to use array?
struct canfd_fifo {
  uint32_t header1;
  uint32_t header2;
  uint32_t data_word1;
  uint32_t data_word2;
};
typedef struct canfd_fifo canfd_fifo;

// NOT NEEDED? For usb
typedef struct
{
  __IO uint32_t RIR;  /*!< CAN receive FIFO mailbox identifier register */
  __IO uint32_t RDTR; /*!< CAN receive FIFO mailbox data length control and time stamp register */
  __IO uint32_t RDLR; /*!< CAN receive FIFO mailbox data low register */
  __IO uint32_t RDHR; /*!< CAN receive FIFO mailbox data high register */
} CAN_FIFOMailBox_TypeDef;

FDCAN_GlobalTypeDef *cans[] = {FDCAN1, FDCAN2, FDCAN3};

bool can_set_speed(uint8_t can_number) {
  bool ret = true;
  FDCAN_GlobalTypeDef *CANx = CANIF_FROM_CAN_NUM(can_number);
  uint8_t bus_number = BUS_NUM_FROM_CAN_NUM(can_number);

  ret &= llcan_set_speed(CANx, can_speed[bus_number], can_data_speed[bus_number], can_loopback, (unsigned int)(can_silent) & (1U << can_number));
  return ret;
}

void can_set_gmlan(uint8_t bus) {
  UNUSED(bus);
  puts("GMLAN not available on red panda\n");
}

void cycle_transceiver(uint8_t can_number) {
  // FDCAN1 = trans 1, FDCAN3 = trans 3, FDCAN2 = trans 2 normal or 4 flipped harness
  uint8_t transceiver_number = can_number;
  if (can_number == 2U) {
    uint8_t flip = (car_harness_status == HARNESS_STATUS_FLIPPED) ? 2U : 0U;
    transceiver_number += flip;
  }
  current_board->enable_can_transceiver(transceiver_number, false);
  delay(20000);
  current_board->enable_can_transceiver(transceiver_number, true);
  bus_off_err[can_number] = 0U;
  puts("Cycled transceiver number: "); puth(transceiver_number); puts("\n");
}

// ***************************** CAN *****************************
void process_can(uint8_t can_number) {
  if (can_number != 0xffU) {
    ENTER_CRITICAL();

    FDCAN_GlobalTypeDef *CANx = CANIF_FROM_CAN_NUM(can_number);
    uint8_t bus_number = BUS_NUM_FROM_CAN_NUM(can_number);
    
    CANx->IR |= FDCAN_IR_TFE; // Clear Tx FIFO Empty flag

    if ((CANx->TXFQS & FDCAN_TXFQS_TFQF) == 0) {
      CANPacket_t to_send;
      if (can_pop(can_queues[bus_number], &to_send)) {
        can_tx_cnt += 1;
        uint32_t TxFIFOSA = FDCAN_START_ADDRESS + (can_number * FDCAN_OFFSET) + (FDCAN_RX_FIFO_0_EL_CNT * FDCAN_RX_FIFO_0_EL_SIZE);
        uint8_t tx_index = (CANx->TXFQS >> FDCAN_TXFQS_TFQPI_Pos) & 0x1F;
        // only send if we have received a packet
        canfd_fifo *fifo;
        fifo = (canfd_fifo *)(TxFIFOSA + (tx_index * FDCAN_TX_FIFO_EL_SIZE));

        // Convert from "mailbox type"
        //fifo->RIR = ((to_send.RIR & 0x6) << 28) | (to_send.RIR >> 3);  // identifier format and frame type | identifier
        //REDEBUG: enable CAN FD and BRS for test purposes
        ////fifo->RDTR = ((to_send.RDTR & 0xF) << 16) | ((to_send.RDTR) >> 16) | (1U << 21) | (1U << 20); // DLC (length) | timestamp | enable CAN FD | enable BRS
        //fifo->RDTR = ((to_send.RDTR & 0xF) << 16) | ((to_send.RDTR) >> 16); // DLC (length) | timestamp
        //fifo->RDLR = to_send.RDLR;
        //fifo->RDHR = to_send.RDHR;

        fifo->header1 = (to_send.extended << 30) | ((to_send.extended != 0) ? (to_send.addr) : (to_send.addr << 18));
        fifo->header2 = (to_send.len << 16); // DLC(length)
        fifo->data_word1 = to_send.data[0] | (to_send.data[1] << 8) | (to_send.data[2] << 16) | (to_send.data[3] << 24);
        fifo->data_word2 = to_send.data[4] | (to_send.data[5] << 8) | (to_send.data[6] << 16) | (to_send.data[7] << 24);

        CANx->TXBAR = (1UL << tx_index); 

        // Send back to USB
        can_txd_cnt += 1;
        CANPacket_t to_push;
        // to_push.RIR = to_send.RIR;
        // to_push.RDTR = (to_send.RDTR & 0xFFFF000FU) | ((CAN_BUS_RET_FLAG | bus_number) << 4);
        // to_push.RDLR = to_send.RDLR;
        // to_push.RDHR = to_send.RDHR;
        to_push.returned = 1U;
        to_push.extended = to_send.extended;
        to_push.addr = to_send.addr;
        to_push.bus = to_send.bus;
        to_push.len = to_send.len;
        memcpy(to_push.data, to_send.data, sizeof(to_push.data));
        can_send_errs += can_push(&can_rx_q, &to_push) ? 0U : 1U;

        usb_cb_ep3_out_complete();
      }
    }

    // Recover after Bus-off state
    if (((CANx->PSR & FDCAN_PSR_BO) != 0) && ((CANx->CCCR & FDCAN_CCCR_INIT) != 0)) {
      bus_off_err[can_number] += 1U;
      puts("CAN is in Bus_Off state! Resetting... CAN number: "); puth(can_number); puts("\n");
      if (bus_off_err[can_number] > BUS_OFF_FAIL_LIMIT) {
        cycle_transceiver(can_number);
      }
      CANx->IR = 0xFFC60000U; // Reset all flags(Only errors!)
      CANx->CCCR &= ~(FDCAN_CCCR_INIT);
      uint32_t timeout_counter = 0U;
      while((CANx->CCCR & FDCAN_CCCR_INIT) != 0) {
        // Delay for about 1ms
        delay(10000);
        timeout_counter++;

        if(timeout_counter >= CAN_INIT_TIMEOUT_MS){
          puts(CAN_NAME_FROM_CANIF(CANx)); puts(" Bus_Off reset timed out!\n");
          break;
        }
      }
    }
    EXIT_CRITICAL();
  }
}

// CAN receive handlers
// blink blue when we are receiving CAN messages
void can_rx(uint8_t can_number) {
  FDCAN_GlobalTypeDef *CANx = CANIF_FROM_CAN_NUM(can_number);
  uint8_t bus_number = BUS_NUM_FROM_CAN_NUM(can_number);
  uint8_t rx_fifo_idx;

  // Rx FIFO 0 new message
  if((CANx->IR & FDCAN_IR_RF0N) != 0) {
    CANx->IR |= FDCAN_IR_RF0N;
    while((CANx->RXF0S & FDCAN_RXF0S_F0FL) != 0) {
      can_rx_cnt += 1;

      // can is live
      pending_can_live = 1;

      // getting new message index (0 to 63)
      rx_fifo_idx = (uint8_t)((CANx->RXF0S >> FDCAN_RXF0S_F0GI_Pos) & 0x3F);

      uint32_t RxFIFO0SA = FDCAN_START_ADDRESS + (can_number * FDCAN_OFFSET);
      CANPacket_t to_push;
      canfd_fifo *fifo;

      // getting address
      fifo = (canfd_fifo *)(RxFIFO0SA + (rx_fifo_idx * FDCAN_RX_FIFO_0_EL_SIZE));

      // Need to convert real CAN frame format to mailbox "type"
      // to_push.RIR = ((fifo->RIR >> 28) & 0x6) | (fifo->RIR << 3); // identifier format and frame type | identifier
      // to_push.RDTR = ((fifo->RDTR >> 16) & 0xF) | (fifo->RDTR << 16); // DLC (length) | timestamp
      // to_push.RDLR = fifo->RDLR;
      // to_push.RDHR = fifo->RDHR;

      to_push.returned = 0U;
      to_push.extended = (fifo->header1 >> 30) & 1U;
      to_push.addr = ((to_push.extended != 0) ? (fifo->header1 & 0x1FFFFFFFU) : ((fifo->header1 >> 18) & 0x7FFU));
      to_push.bus = bus_number;
      to_push.len = ((fifo->header2 >> 16) & 0xFU);
      to_push.data[0] = fifo->data_word1 & 0xFFU;
      to_push.data[1] = (fifo->data_word1 >> 8U) & 0xFFU;
      to_push.data[2] = (fifo->data_word1 >> 16U) & 0xFFU;
      to_push.data[3] = (fifo->data_word1 >> 24U) & 0xFFU;
      to_push.data[4] = fifo->data_word2 & 0xFFU;
      to_push.data[5] = (fifo->data_word2 >> 8U) & 0xFFU;
      to_push.data[6] = (fifo->data_word2 >> 16U) & 0xFFU;
      to_push.data[7] = (fifo->data_word2 >> 24U) & 0xFFU;

      // modify RDTR for our API
      //to_push.RDTR = (to_push.RDTR & 0xFFFF000F) | (bus_number << 4);

      // forwarding (panda only)
      int bus_fwd_num = (can_forwarding[bus_number] != -1) ? can_forwarding[bus_number] : safety_fwd_hook(bus_number, &to_push);
      if (bus_fwd_num != -1) {
        CANPacket_t to_send;
        // to_send.RIR = to_push.RIR;
        // to_send.RDTR = to_push.RDTR;
        // to_send.RDLR = to_push.RDLR;
        // to_send.RDHR = to_push.RDHR;
        to_send.returned = 0U;
        to_send.extended = to_push.extended;
        to_send.addr = to_push.addr;
        to_send.bus = to_push.bus;
        to_send.len = to_push.len;
        memcpy(to_send.data, to_push.data, sizeof(to_send.data));
        can_send(&to_send, bus_fwd_num, true);
      }

      can_rx_errs += safety_rx_hook(&to_push) ? 0U : 1U;
      ignition_can_hook(&to_push);

      current_board->set_led(LED_BLUE, true);
      can_send_errs += can_push(&can_rx_q, &to_push) ? 0U : 1U;

      // update read index 
      CANx->RXF0A = rx_fifo_idx;
    }

  } else if((CANx->IR & (FDCAN_IR_PEA | FDCAN_IR_PED | FDCAN_IR_RF0L | FDCAN_IR_RF0F | FDCAN_IR_EW | FDCAN_IR_MRAF | FDCAN_IR_TOO)) != 0) {
    #ifdef DEBUG
      puts("FDCAN error, FDCAN_IR: ");puth(CANx->IR);puts("\n");
    #endif
    CANx->IR |= (FDCAN_IR_PEA | FDCAN_IR_PED | FDCAN_IR_RF0L | FDCAN_IR_RF0F | FDCAN_IR_EW | FDCAN_IR_MRAF | FDCAN_IR_TOO); // Clean all error flags
    can_err_cnt += 1;
  } else { 
    
  }
  
}

void FDCAN1_IT0_IRQ_Handler(void) { can_rx(0); }
void FDCAN1_IT1_IRQ_Handler(void) { process_can(0); }

void FDCAN2_IT0_IRQ_Handler(void) { can_rx(1); }
void FDCAN2_IT1_IRQ_Handler(void) { process_can(1); }

void FDCAN3_IT0_IRQ_Handler(void) { can_rx(2);  }
void FDCAN3_IT1_IRQ_Handler(void) { process_can(2); }

bool can_init(uint8_t can_number) {
  bool ret = false;

  REGISTER_INTERRUPT(FDCAN1_IT0_IRQn, FDCAN1_IT0_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_1)
  REGISTER_INTERRUPT(FDCAN1_IT1_IRQn, FDCAN1_IT1_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_1)
  REGISTER_INTERRUPT(FDCAN2_IT0_IRQn, FDCAN2_IT0_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_2)
  REGISTER_INTERRUPT(FDCAN2_IT1_IRQn, FDCAN2_IT1_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_2)
  REGISTER_INTERRUPT(FDCAN3_IT0_IRQn, FDCAN3_IT0_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_3)
  REGISTER_INTERRUPT(FDCAN3_IT1_IRQn, FDCAN3_IT1_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_3)

  if (can_number != 0xffU) {
    FDCAN_GlobalTypeDef *CANx = CANIF_FROM_CAN_NUM(can_number);
    ret &= can_set_speed(can_number);
    ret &= llcan_init(CANx);
    // in case there are queued up messages
    process_can(can_number);
  }
  return ret;
}
