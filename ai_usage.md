Tool used: Gemini (Google)

Pe baza structurii tale Report și a specificațiilor proiectului (câmpurile severity, category, inspector, timestamp și operatorii ==, !=, <, <=, >, >=), am generat cele două funcții.

Iată codul și explicațiile necesare pentru a le putea prezenta cu brio profesorilor.

1. Funcția parse_condition
Această funcție ia un string de tipul "severity:>=:2" și îl sparge în trei bucăți. Cea mai eficientă metodă în C pentru acest format fix este folosirea funcției sscanf.

int parse_condition(const char *input, char *field, char *op, char *value) {
    // Folosim sscanf pentru a extrage datele dintr-un format fix.
    // %[^:] inseamna "citeste absolut toate caracterele pana cand intalnesti caracterul ':'"
    // Ultimul %s citeste restul string-ului.
    
    if (sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3) {
        return 1; // Succes: am extras exact 3 elemente
    }
    
    return 0; // Eroare: string-ul nu a respectat formatul "a:b:c"
}
Cum o folosești: Înainte să o apelezi, trebuie să declari trei buffere goale în care funcția să pună rezultatele: char field[50], op[5], value[100];.

2. Funcția match_condition
Aici este nucleul filtrării. Această funcție primește o structură Report și cele trei string-uri extrase anterior. Ea trebuie să transforme valoarea din string (ex: "2") într-un număr (int) și să facă logica matematică.

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    
    // 1. Verificam pentru SEVERITY (are nevoie de conversie in INT)
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value); // Transformam string-ul in numar intreg
        
        if (strcmp(op, "==") == 0) return r->severity_level == val;
        if (strcmp(op, "!=") == 0) return r->severity_level != val;
        if (strcmp(op, "<")  == 0) return r->severity_level < val;
        if (strcmp(op, "<=") == 0) return r->severity_level <= val;
        if (strcmp(op, ">")  == 0) return r->severity_level > val;
        if (strcmp(op, ">=") == 0) return r->severity_level >= val;
    }
    
    // 2. Verificam pentru TIMESTAMP (are nevoie de conversie in LONG)
    else if (strcmp(field, "timestamp") == 0) {
        long val = atol(value); // Transformam string-ul in long
        
        if (strcmp(op, "==") == 0) return r->timestamp == val;
        if (strcmp(op, "!=") == 0) return r->timestamp != val;
        if (strcmp(op, "<")  == 0) return r->timestamp < val;
        if (strcmp(op, "<=") == 0) return r->timestamp <= val;
        if (strcmp(op, ">")  == 0) return r->timestamp > val;
        if (strcmp(op, ">=") == 0) return r->timestamp >= val;
    }
    
    // 3. Verificam pentru CATEGORY (comparare de string-uri)
    else if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->issue_category, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->issue_category, value) != 0;
    }
    
    // 4. Verificam pentru INSPECTOR (comparare de string-uri folosind 'name')
    else if (strcmp(field, "inspector") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->name, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->name, value) != 0;
    }

    // Daca ajungem aici, inseamna ca field-ul sau operatorul sunt invalide
    return 0; 
}

