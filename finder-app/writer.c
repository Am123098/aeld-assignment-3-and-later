#include <stdio.h>
#include <sys/syslog.h>
#include <syslog.h>
#include <stdlib.h>

int main(int argc, char** argv) {

    openlog("Writer-Util", LOG_PID | LOG_CONS, LOG_USER);
    
    if (argc < 3) {
        syslog(LOG_ERR, "Invalid number of arguments!! Expected: writer [File Path] [Text] -- given that the filepath is valid");
        closelog();
        return EXIT_FAILURE; 
    }

    const char* filepath = argv[1];
    const char* string = argv[2];

    FILE* file = fopen(filepath, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Failed to open file : %s", filepath); 
        closelog();
        return EXIT_FAILURE;
    }

    if (fputs(string, file) == EOF) {
        syslog(LOG_ERR, "Failed to write the content to the file: %s", filepath);
        fclose(file);
        closelog();
        return EXIT_FAILURE;
    }

    syslog(LOG_DEBUG, "Writing %s to file %s", string, filepath);

    fclose(file);
    closelog();
    return EXIT_SUCCESS;
}
