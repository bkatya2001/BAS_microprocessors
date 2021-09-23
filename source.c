#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <string.h>

#define FOSC 1000000
#define BAUD 19219 // 20833
#define MYUBRR FOSC/16/BAUD-1

// ������� ��� ������ � ������
#define ClearBit(reg, bit) reg &= (~(1<<(bit)))
#define SetBit(reg, bit) reg |= (1<<(bit))   

uint8_t signal = 0; // ������� ������ �� ������
bool is_started = false; // ��� �� ��������
int timer = 0; // �������� � �������
int i = 0;

char alphabet_rus[42][8] = { "�.-", "�-...", "�.--", "�--.", "�-..", "�.", "�...-", "�--..", "�..", "�.---",
  "�-.-", "�.-..", "�--", "�-.", "�---", "�.--.", "�.-.", "�...", "�-", "�..-", "�..-.", "�....", "�-.-.", "�---.",
  "�----", "�--.-", "�.--.-.", "�-.--", "�-..-", "�..-..", "�..--", "�.-.-", "1.----", "2..---", "3...--", "4....-",
  "5.....", "6-....", "7--...", "8---..", "9----.", "0-----" };
char alphabet_eng[36][8] = { "A.-", "B-...", "C-.-.", "D-..", "E.", "F..-.", "G--.", "H....", "I..", "J.---",
   "K-.-", "L.-..", "M--", "N-.", "O---", "P.--.", "Q--.-", "R.-.", "S...", "T-", "U..-", "V...-", "W.--", "X-..-",
   "Y-.--", "Z--..", "1.----", "2..---", "3...--", "4....-", "5.....", "6-....", "7--...", "8---..", "9----.", "0-----" };

char message[896] = ""; // ����� � ����
char result[256] = ""; // �����

// ������� ������ �������
void LcdWriteCom(unsigned char data)
{
    ClearBit(PORTC, 5); // ������������� RS � 0
    PORTB = data; // ������� ������ �� ����   
    SetBit(PORTC, 0); // ������������� � � 1
    _delay_us(2);
    ClearBit(PORTC, 0); // ������������� � � 0
    _delay_us(40);
}

// ������� �������������
void InitLcd(void)
{
    DDRC = (1 << DDC5) | (1 << DDC0);
    PORTC = (0 << PORTC5) | (0 << PORTC0);

    DDRB = (1 << DDB7) | (1 << DDB6) | (1 << DDB5) | (1 << DDB4) | (1 << DDB3) | (1 << DDB2) | (1 << DDB1) | (1 << DDB0);
    PORTB = (0 << PORTB7) | (0 << PORTB6) | (0 << PORTB5) | (0 << PORTB4) | (0 << PORTB3) | (0 << PORTB2) | (0 << PORTB1) | (0 << PORTB0);

    PORTC = (1 << PORTC5) | (1 << PORTC0);
    _delay_ms(40);
    LcdWriteCom(0x38); // 0b00111000 - 8 ��������� ����, 2 ������
    LcdWriteCom(0x0f); // 0b00001111 - �������, ������, �������� ��������
    LcdWriteCom(0x01); // 0b00000001 - ������� �������
    _delay_ms(2);
    LcdWriteCom(0x06); // 0b00000110 - ������ �������� ������, ������ ���
}

void ClearMemory(void) {
    int i;
    int len = strlen(message);
    for (i = 0; i < len; i++) message[i] = '\0';
    len = strlen(result);
    for (i = 0; i < len; i++) result[i] = '\0';
    is_started = false;
}

void TransmitData(void) {
    int i = 0;

    if (PIND & (1 << 3)) { // ���������� ������� ��������� �� ��������
        int len = strlen(result);
        for (i = 0; i < len; i++) // ���� ������ ��������� ���� �������� ������ � ���� �B ����������������
        {
            SetBit(PORTC, 5); // ������������� RS � 1
            PORTB = result[i]; // ������ ��������� ���� ���������� ������� ������ � ���� �B
            SetBit(PORTC, 0); // ������������� � � 1
            _delay_us(2);
            ClearBit(PORTC, 0); // ������������� � � 0
            _delay_us(40);
        }
    }
    else {
        // ������������� �������� �������� ������ � �����
        UBRR0H = (MYUBRR >> 8);
        UBRR0L = (MYUBRR);

        UCSR0C = 0x06; // ������ ��������: 8 ��� ������, 1 �������� ���
        UCSR0B = (1 << TXEN0); // ��������� ��������

        for (i = 0; i < strlen(result); i++)
        {
            while (!(UCSR0A & (1 << UDRE0))); // ��� ������������ ������
            UDR0 = result[i]; // �������� ������
        }
    }
}

void DecodeMessage(void) {
    char(*alphabet)[8];
    int k = 0;
    int begin = 0;
    size_t n = strlen(message);
    size_t len;

    if (PIND & (1 << 4)) { // ����������� ����� �� ��������
        alphabet = alphabet_eng;
        len = 36;
    }
    else {
        alphabet = alphabet_rus;
        len = 42;
    }

    int i;
    for (i = 0; i < n; i++) {
        if (message[i] == '*' || message[i] == ' ' || i + 1 == n) {
            char letter[7] = "";
            if (i + 1 == n) strncpy(letter, message + begin, i - begin + 1);
            else strncpy(letter, message + begin, i - begin);
            int j;
            for (j = 0; j < len; j++) {
                if (strcmp(letter, alphabet[j] + 1) == 0) {
                    result[k] += alphabet[j][0];
                    k++;
                    break;
                }
            }
            begin = i + 1;
            if (message[i] == ' ') {
                result[k] = ' ';
                k++;
            }
        }
    }
}

ISR(TIMER1_OVF_vect) // ���������� �������
{
    TCNT1 = 0;
    is_started = false;
    i = 0;
}

int main(void) {
    // ��������� �������
    TCCR1B = (1 << CS12) | (0 << CS11) | (1 << CS10); // ��/1024
    sei(); // SREG = 1 << I (��������� ����� ����������)
    TIFR1 = 1 << TOV0; // ����� ���������� ����������
    TIMSK1 = 1 << TOIE1; // ���������� ���������� �� ������������
    GTCCR = 1 << PSRSYNC;  // ����� ������������
    TCNT1 = 0;

    DDRD &= ~(1 << PD6); // ��������� PD6 �� ����
    DDRD &= ~(1 << PD3);
    DDRD &= ~(1 << PD4);

    while (1) {
        uint8_t new_pin = PIND & (1 << 6);
        timer = TCNT1;
        if (new_pin == 0 && signal != 0 && is_started) { // ���� ������� � 1 � 0
            if (timer >= 87 && timer < 175) {
                message[i] = '.';
            }
            else if (timer >= 175 && timer <= 253) {
                message[i] = '-';
            }
            else message[i] = '?';
            i++;
            TCNT1 = 0;
        }
        else if (new_pin != 0 && signal == 0) { // ���� ������� � 0 � 1
            if (is_started) {
                if (timer >= 87 && timer < 175) {}
                else if (timer >= 263 && timer <= 380) {
                    message[i] = '*';
                    i++;
                }
                else if (timer >= 614 && timer <= 888) {
                    message[i] = ' ';
                    i++;
                }
                else {
                    message[i] = '?';
                    i++;
                }
                TCNT1 = 0;
            }
            else {
                is_started = true;
            }
        }
        else if (new_pin == 0 && timer > 930 && is_started) {
            TCNT1 = 0;
            is_started = false;
            i = 0;

            InitLcd();
            DecodeMessage();
            TransmitData();
            ClearMemory();
        }
        signal = new_pin;
    }
    return 0;
}