Tool used: Gemini (Google)

Based on the Report structure and the project specifications (fields: severity, category, inspector, timestamp, and operators: ==, !=, <, <=, >, >=), here are the two generated functions.

Below is the code and the necessary explanations to present them successfully.

1. The parse_condition Function
This function takes a string such as "severity:>=:2" and splits it into three parts. The most efficient method in C for this fixed format is using the sscanf function.

int parse_condition(const char *input, char *field, char *op, char *value) {
    // We use sscanf to extract data from a fixed format.
    // %[^:] means "read absolutely all characters until you encounter the ':' character".
    // The last %s reads the remainder of the string.
    
    if (sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3) {
        return 1; // Success: extracted exactly 3 elements
    }
    
    return 0; // Error: the string did not match the "a:b:c" format
}
How to use it: Before calling this function, you must declare three empty buffers where the function will store the parsed results: char field[50], op[5], value[100];.

2. The match_condition Function
This is the core of the filtering logic. This function receives a Report structure and the three previously extracted strings. It is responsible for converting the value from the string (e.g., "2") into a numerical type (int or long) and applying the mathematical logic.

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    
    // 1. Check for SEVERITY (requires conversion to INT)
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value); // Convert the string to an integer
        
        if (strcmp(op, "==") == 0) return r->severity_level == val;
        if (strcmp(op, "!=") == 0) return r->severity_level != val;
        if (strcmp(op, "<")  == 0) return r->severity_level < val;
        if (strcmp(op, "<=") == 0) return r->severity_level <= val;
        if (strcmp(op, ">")  == 0) return r->severity_level > val;
        if (strcmp(op, ">=") == 0) return r->severity_level >= val;
    }
    
    // 2. Check for TIMESTAMP (requires conversion to LONG)
    else if (strcmp(field, "timestamp") == 0) {
        long val = atol(value); // Convert the string to a long
        
        if (strcmp(op, "==") == 0) return r->timestamp == val;
        if (strcmp(op, "!=") == 0) return r->timestamp != val;
        if (strcmp(op, "<")  == 0) return r->timestamp < val;
        if (strcmp(op, "<=") == 0) return r->timestamp <= val;
        if (strcmp(op, ">")  == 0) return r->timestamp > val;
        if (strcmp(op, ">=") == 0) return r->timestamp >= val;
    }
    
    // 3. Check for CATEGORY (string comparison)
    else if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->issue_category, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->issue_category, value) != 0;
    }
    
    // 4. Check for INSPECTOR (string comparison using the 'name' field)
    else if (strcmp(field, "inspector") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->name, value) == 0;
        if (strcmp(op, "!=") == 0) return strcmp(r->name, value) != 0;
    }

    // If we reach this point, it means the field or operator is invalid
    return 0; 
}

    The solution that Gemini produced was correct and didn't need alteration (surprisingly). The only problem I ran into was the fact that when i was calling a function with the follwing command: "./prog city_manager --role inspector --user bob --filter downtown severity:>=:2 category:==:road", it gives the error: "zsh: :2 not found", so I had to use it with "./prog city_manager --role inspector --user bob --filter downtown "severity:>=:2" "category:==:road"".
    The other modifications I made were purely aethetic, deleting spaces and indenting where I tought it was neccessary and also deteling the comments.