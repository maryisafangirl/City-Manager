Tool used: Gemini (Google)

PHASE 1

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

PHASE 2
    I promted the AI to explain in detail each field of the sigaction struct used for monitoring the reports. I have also prompted it to generate different test cases in which my program might fail or not produce the expected outcome in order to thoroughly test it.

AI response:
    Edge Cases (Boundary scenarios where the program might crash or fail)

1. Using printf inside the Handler

Problem: Technically speaking (at a very advanced level), the printf function is not async-signal-safe. Why? Because it uses internal lock mechanisms (memory locking) to print correctly to the screen. If a signal interrupts another printf from main(), and then the handler also calls printf, the program can enter a complete deadlock.

Industry Solution: Instead of printf, handlers use write(1, "Message\n", 6); (which writes directly to the standard output at the system level). You can change that printf to a simple write().

2. The program is killed by Brute Force (SIGKILL)

Problem: If someone issues the command kill -9 <PID> (which sends the SIGKILL signal), the operating system kills your program instantly, as if cutting its power. The program no longer has time to "catch" the signal with sigaction, so the unlink(".monitor_pid") line will not be executed.

Result: The .monitor_pid file remains on the disk (stale PID file).

Solution (already implemented): The fact that you used the O_TRUNC flag in the open function solves the problem! The next time someone manually starts the monitor, it will quietly overwrite that orphaned file, respecting the "creates or overwrites" requirement.  

PHASE 3
I made the AI generate edge test cases.
Phase 1: Input & Data Integrity TestsTest 1: The Long Description OverflowAction: Run city_manager --add, and when prompted for the description, paste a block of text that is 500 characters long.What to look for: Since your Report struct allocates char description[256];, writing past 255 characters might cause a buffer overflow or corrupt the next record.  Quick Fix: Limit scanf by changing scanf("%[^\n]", ...) to scanf("%255[^\n]", report.description);.Test 2: The "Permission Sabotage"Action: Run chmod 000 downtown/district.cfg in your terminal to completely strip all read/write permissions from the file. Then, try to run update_threshold downtown 3 as a manager.What to look for: Your open(file_path, O_RDONLY | O_WRONLY) call should gracefully fail and print "Error opening the file!" instead of crashing.  Phase 2: Process & Concurrency TestsTest 3: The Shell Injection DefenseAction: Run city_manager --role manager --user alice --remove_district "downtown; ls -l".What to look for: Does it execute ls -l? Because you correctly used execlp("rm", "rm", "-rf", district_id, NULL), it entirely bypasses the system shell. It will safely attempt (and fail) to delete a folder literally named "downtown; ls -l". This proves your code is highly secure!  Test 4: The PID Reincarnation (Extreme Edge Case)Action: Open .monitor_pid manually and type a PID of a completely different running process (e.g., type 1 for the OS init process, or find your browser's PID). Then run ./city_hub and start_monitor.What to look for: Your program will say Another monitor is already open.... It assumes that because kill(old_pid, 0) == 0 succeeds, the monitor is alive. Phase 3: Hub Stability TestsTest 5: The Ctrl+D (EOF) Death TrapAction: Start ./city_hub. Instead of typing a command, press Ctrl + D.What to look for: Ctrl + D sends an End-of-File (EOF) signal. fgets will return NULL. If unchecked, your program will attempt buffer[strcspn(buffer, "\n")] = 0 on stale or uninitialized memory, causing a Segmentation Fault or infinite loop.  Quick Fix: In city_hub.c, right after fgets, add: if (feof(stdin)) { printf("\nExiting hub...\n"); exit(0); }.Test 6: The Broken Pipe (SIGPIPE)Action: Inside city_hub, type calculate_scores downtown. While it is calculating, quickly press Ctrl+C in a way that kills the reader (city_hub), but leaves the scorer running and trying to write to a pipe that has no reader.What to look for: UNIX usually sends a SIGPIPE signal to a process writing to a broken pipe, which kills it automatically. Your architecture is fundamentally immune to dangling writer processes because of this OS-level protection.

Advanced Edge Case Tests
To ensure your code is completely foolproof and resilient against unexpected user behavior or system states, run the following specialized test cases.

1. The "Signal Flood" (Testing UNIX Signal Coalescing)
Standard UNIX signals do not queue; if multiple signals arrive before the process wakes up, they are merged into a single signal.

How to test: Start the monitor from city_hub. Open a new terminal and run the city_manager --add command 10 times in incredibly rapid succession (you can do this by chaining them in bash: ./city_manager ... --add downtown & ./city_manager ... & ...).

Expected result: The monitor might only print [Monitor] A new report has been added to the system! a few times, not exactly 10 times. This is expected OS behavior, but your city_hub pipeline must not crash, buffer-overflow, or freeze while receiving these rapid bursts.

2. The Corrupted reports.dat File (Testing Struct Size Safety)
Your scorer.c assumes that reports.dat is perfectly divisible by sizeof(Report). What if the file is interrupted mid-write or manually altered?

How to test: Run start_monitor. In a separate terminal, forcefully truncate the reports file to an invalid byte size: truncate -s 100 downtown/reports.dat. Then, run calculate_scores downtown in your hub.

Expected result: Because read(fd, &r, sizeof(Report)) expects a full block, it should safely fail to read the partial block at the end. However, verify that your scorer does not print garbage data for the last "half-read" inspector and that it exits cleanly without a Segmentation Fault.

3. The "Missing Executable" Trap (Testing Exec Failure)
If the external programs are deleted, the system must not leave zombie processes or dangling pipes.

How to test: Delete or rename the scorer executable (mv scorer scorer_hidden). Run ./city_hub and execute calculate_scores downtown.

Expected result: The execlp("./scorer", ...) call will fail. The perror will print to the screen, and the child process will exit(-1). The parent (city_hub) must gracefully catch the closed pipe, return to the >hub prompt, and not hang forever waiting for waitpid().

4. The "Maximum Districts" Stress Test (Testing File Descriptor Limits)
Every time calculate_scores runs, it creates pipes. If you run it on too many districts at once, you might hit the operating system's open file limit.

How to test: Type calculate_scores followed by 50 random district names (e.g., calculate_scores d1 d2 d3 d4 ... d50).

Expected result: Since your calculate_scores implementation waits for the child process using waitpid() inside the for loop, it closes the file descriptors sequentially before opening the next ones. It should successfully handle an infinite amount of districts without throwing a "Too many open files" error.

5. The Zero-Byte PID Ghost (Testing atoi Resilience)
You already handled the ghost PID where the process is dead, but what if the file exists but is completely empty?

How to test: Run touch .monitor_pid (creating a 0-byte file). Start the city_hub and run start_monitor.

Expected result: The read function in monitor_reports.c will return 0 bytes. The buff remains empty (all zeroes), so atoi(buff) returns 0. The condition if (old_pid > 0 && kill(old_pid, 0) == 0) will safely evaluate to false, allowing the monitor to overwrite the empty file with its new, valid PID.

The program passed all of the test except the one with the corrupted reports.dat file. I implementet a way to avoid this problem in the future. 