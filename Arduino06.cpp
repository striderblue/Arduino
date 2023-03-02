const int AB_max = 10;
const char *AB_str[AB_max] = {"A001"};
int AB_count = 1;

void AB_add(const char *s) {
	if (AB_count < AB_max) {
		AB_str[AB_count++] = s;
	}
	else {
		Serial.println("Data exceeded!");
	}
}

void AB_remove(const char *s) {
	int i;
	for (i = 0; i < AB_count; i++) {
		if (strcmp(s, AB_str[i]) == 0) break;
	}
	
	if (i == AB_count) return;
	
	memmove(&AB_str[i], &AB_str[i+1], (AB_count - (i + 1)) * sizeof *AB_str);
	AB_count--;
}

void AB_print() {
	Serial.print("AB: ");
	Serial.print(AB_count);
	Serial.println(" items");
	for (int i = 0; i < AB_count; i++) {
		Serial.print("     ");
		Serial.println(AB_str[i]);
	}
	Serial.println();
}

void setup() {
	Serial.begin(9600);
	AB_print();
	AB_add("A002");
	AB_print();
	AB_remove("A001");
	AB_print();
}

void loop() {

}

// Save Queue Array
const int Queue_Max = 10;
const char *Queue_str[Queue_Max] = {};
int Queue_count = 0;
