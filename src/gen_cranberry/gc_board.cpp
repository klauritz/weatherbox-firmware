#include "gc_board.h"

static void gc_board_print_build_opts();
static void gc_board_setup(struct gc_board* b);
static void gc_board_post();

static void gc_board_run_cmd(struct gc_board* b);
static int gc_board_ready_run_cmd(struct gc_board* b);

static void gc_board_sample(struct gc_board* b);
static int gc_board_ready_sample(struct gc_board* b);

static void gc_board_tx(struct gc_board* b);
static int gc_board_ready_tx(struct gc_board* b);

static int gc_board_ready_heartbeat_tx(struct gc_board* b);
static void gc_board_heartbeat_tx(struct gc_board* b);

void gc_board_init(gc_board *b){
    // Link functions to make them accessable
    b->print_build_opts = &gc_board_print_build_opts;
    b->setup = &gc_board_setup;
    b->post = &gc_board_post;

    // Sample and TX module
    b->tx = &gc_board_tx;
    b->ready_tx = &gc_board_ready_tx;
    b->sample = &gc_board_sample;
    b->ready_sample = &gc_board_ready_sample;

    // CMD Parsing Module
    b->run_cmd = &gc_board_run_cmd;
    b->ready_run_cmd = &gc_board_ready_run_cmd;

    // Heartbeat Module
    b->ready_heartbeat_tx = &gc_board_ready_heartbeat_tx;
    b->heartbeat_tx = &gc_board_heartbeat_tx;

    // State Variables
    b->sample_count = 0;
    b->node_addr = 0;
    b->prev_sample_ms = 0;

    // Initialize the packet
    b->data_packet.schema = 0;
    b->data_packet.node_addr = 0;
    b->data_packet.uptime_ms = 0;
    b->data_packet.batt_mv = 0;
    b->data_packet.panel_mv = 0;
    b->data_packet.bmp085_press_pa = 0;
    b->data_packet.bmp085_temp_decic = 0;
    b->data_packet.humidity_centi_pct = 0;
    b->data_packet.apogee_w_m2 = 0;
}

static void gc_board_print_build_opts()
{
    Serial.begin(9600);
    Serial.println("Board Opts");
    Serial.println("Gen: cranberry");
}

static void gc_board_setup(struct gc_board* b){
    Serial.begin(9600);
    Serial.println("Board Setup Start");

    // Open Devices
    gc_dev_xbee_open();
    gc_dev_sht1x_open();
    gc_dev_bmp085_open();
    gc_dev_apogee_sp212_open();
    gc_dev_batt_open();
    gc_dev_spanel_open();
    gc_dev_eeprom_naddr_open();

    // load the address from the hardware
    b->node_addr = gc_dev_eeprom_naddr_read();

    delay(100);
    Serial.println("Board Setup Done");
}

// power on self test
static void gc_board_post(){
    Serial.println("POST Begin");

    // Display node addr
    Serial.print("P: node addr: ");
    Serial.println((int) gc_dev_eeprom_naddr_read());

    // Check sht1x
    Serial.println("P: Check sht1x value");
    int sht1x_val = gc_dev_sht1x_read();

    Serial.print("P: sht1x value: ");
    Serial.print(sht1x_val);
    Serial.println("\%");

    if(sht1x_val < 0){
        Serial.println("P: Error: Humidity out of range");
    }

    // Check BMP085
    Serial.println("P: Check bmp085 value");
    int32_t bmp085_val = gc_dev_bmp085_read();

    Serial.print("P: bmp085 value: ");
    Serial.print(bmp085_val/10);
    Serial.print(".");
    Serial.print((bmp085_val-bmp085_val/10)/1000);
    Serial.println(" mb");

    if(bmp085_val < 80000){
        Serial.println("P: Error: bmp085 pressure out of range");
    }

    // Check BMP085 temperature
    Serial.println("P: Check bmp085 temp");
    uint16_t bmp085_temp = gc_dev_bmp085_read_temp();

    Serial.print("P: bmp085 temp: ");
    Serial.print(bmp085_temp/10);
    Serial.print(".");
    Serial.print((bmp085_temp-bmp085_temp/10)/10);
    Serial.println(" celsius");

    if(bmp085_temp < 0){
        Serial.println("P: Error: bmp085 temperature out of range");
    }

    // Check apogee_sp212
    Serial.println("P: Check apogee_sp212 value");
    int apogee_sp212_val = gc_dev_apogee_sp212_read();

    Serial.print("P: apogee_sp212 solar irr value: ");
    Serial.print(apogee_sp212_val*(5000/1023));
    Serial.println(" mV");

    if(apogee_sp212_val < 0){
        Serial.println("P: Error: apogee solar irr out of range");
    }

    // Check batt
    Serial.println("P: Check batt value");
    int batt_val = gc_dev_batt_read();

    Serial.print("P: batt value: ");
    Serial.print(batt_val*(5000/1023));
    Serial.println(" mV");

    if(batt_val < 0){
        Serial.println("P: Error: batt out of range");
    }

    // check panel sensor value
    Serial.println("P: check panel sensor value");
    int spanel_val = gc_dev_spanel_read();
    Serial.print("P: spanel value: ");
    Serial.print(2*spanel_val*(5000/1023)+70);
    Serial.println(" mV");

    if(spanel_val < 100){
        Serial.println("P: ERROR: spanel value out of range");
    }

    Serial.println("POST End");

}

static void gc_board_sample(struct gc_board* b){
    Serial.println("Sample Start");
    Serial.println(b->sample_count);

    struct gc_packet* data_packet = &(b->data_packet);
    data_packet->uptime_ms           = millis();
    data_packet->batt_mv             = gc_dev_batt_read();
    data_packet->panel_mv            = gc_dev_spanel_read();
    data_packet->bmp085_press_pa     = gc_dev_bmp085_read();
    data_packet->bmp085_temp_decic   = gc_dev_bmp085_read_temp();
    data_packet->humidity_centi_pct  = gc_dev_sht1x_read();
    data_packet->apogee_w_m2         = gc_dev_apogee_sp212_read();

    Serial.println("Sample End");
    b->sample_count++;
}

static int gc_board_ready_tx(struct gc_board* b){
    const int max_samples = 20;
    if(b->sample_count > max_samples-1){
        return 1;
    }
    else{
        return 0;
    }
}

static int gc_board_ready_sample(struct gc_board* b){
    const int wait_ms = 3000;
    const int sample_delta = millis() - b->prev_sample_ms;

    if( sample_delta >= wait_ms){
        b->prev_sample_ms = millis();
        return 1;
    }
    else{
        return 0;
    }
}

static int gc_board_ready_run_cmd(struct gc_board* b){
    return Serial.available();
}

static void gc_board_run_cmd(struct gc_board* b){
    Serial.println("Enter CMD Mode");
    while(Serial.read() != '\n');
    while(1){
        if(Serial.available()){
            char input = Serial.read();
            Serial.print("GOT A CMD: ");
            Serial.println(input);
            while(Serial.read() != '\n');
            if(input == 'E') {
                break;
            }
            else{
                switch(input){
                    case 'T':
                        Serial.println("CMD Mode cmd");
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

static int gc_board_ready_heartbeat_tx(struct gc_board* b){
    const int wait_ms = 3000;
    int sample_delta = millis() - b->prev_heartbeat_ms;

    unsigned long max_heartbeat_ms = (unsigned long) 1000*69*5;

    // Heartbeats are only enabled for 5 minutes after the
    // device boots up.
    if( millis() < max_heartbeat_ms ){
        if( sample_delta >= wait_ms){
            b->prev_heartbeat_ms = millis();
            return 1;
        }
        else{
            return 0;
        }
    }
    return 0;
}

static void gc_board_heartbeat_tx(struct gc_board* b){
    uint8_t payload[_GC_DEV_XBEE_BUFSIZE_];
    struct gc_heartbeat_packet hb_packet;

    hb_packet.schema = 5;
    hb_packet.uptime_ms = millis();
    hb_packet.batt_mv = gc_dev_batt_read();
    hb_packet.node_addr = gc_dev_eeprom_naddr_read();

    int schema_len = sizeof(hb_packet);

    Serial.println("TX Heartbeat Start");

    // We need to copy our struct data over to a byte array
    // to get a consistent size for sending over xbee.
    // Raw structs have alignment bytes that are in-between the
    // data bytes.
    memset(payload, '\0', sizeof(payload));
    memcpy(payload, &(hb_packet), schema_len);
    gc_dev_xbee_write(payload, schema_len);

    Serial.println("TX Heartbeat End");
}

static void gc_board_tx(struct gc_board* b){
    uint8_t payload[_GC_DEV_XBEE_BUFSIZE_];
    int schema_len = sizeof(b->data_packet);

    Serial.println("Sample TX Start");

    // We need to copy our struct data over to a byte array
    // to get a consistent size for sending over xbee.
    // Raw structs have alignment bytes that are in-between the
    // data bytes.
    memset(payload, '\0', sizeof(payload));
    memcpy(payload, &(b->data_packet), schema_len);
    gc_dev_xbee_write(payload, schema_len);

    // Reset the board sample count so that
    // goes through the sample loop again.
    b->sample_count = 0;

    Serial.println("Sample TX End");
}

static void gc_board_soft_rst(){
    asm volatile ("jmp 0");
}
