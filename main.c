#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ulfius.h>
#include "mjson.h"

#define PORT 8080
#define QUOTE_NO 5421

struct quote_list *list = NULL;

struct quote_node{
    char quote[209]; // set to the largest quote
    char author[29]; // set to the largest author
};

struct quote_list{
    int num; // needs to be a int, shorts don't work with mjson -_-
    struct quote_node nodes[QUOTE_NO];
};

/*
 * Function that is called from an HTTP GET
 * Formats quote and returns it as a plain string
 */
int callback(const struct _u_request *request, struct _u_response *response, void *data){
    srand(time(NULL)/86400);  // Random integer once a day
    int index = rand() % (QUOTE_NO + 1);
    char quote[sizeof(list->nodes[index].quote)];
    strcpy(quote, list->nodes[index].quote);
    strcat(strcat(quote, "\"\n    -"), list->nodes[index].author);
    char final[sizeof(quote)];
    strcpy(final, "\"");
    strcat(strcpy(final, "\""), quote);
    ulfius_set_string_body_response(response, 200, final);
    return U_CALLBACK_CONTINUE;
}

/*
 * Maps JSON quote file into structs
 */
int maplist(const char *buffer, struct quote_list *nodes){
    const struct json_attr_t json_attrs[] = {
        {"quoteText", t_string, STRUCTOBJECT(struct quote_node, quote), .len = sizeof(nodes->nodes[0].quote)},
        {"quoteAuthor", t_string, STRUCTOBJECT(struct quote_node, author), .len = sizeof(nodes->nodes[0].author)},
        {NULL}
    };

    const struct json_attr_t json_list[] = {
        {"class", t_check, .dflt.check = "OBJECTS"},
        {"list", t_array, STRUCTARRAY(nodes->nodes, json_attrs, &(nodes->num))},
        {NULL}
    };
    memset(nodes, '\0', sizeof(*nodes));
    return json_read_object(buffer, json_list, NULL);
}

int main(int argc, char **argv){
    if (argc == 1){
        fprintf(stderr, "Need to supply path to \"quotes.json\" file.\n");
        exit(1);
    }
    FILE *f;
    f = fopen(argv[1], "r");
    if (!f){
        fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
    }
    int readsize, stringsize;
    char *buffer = NULL;

    fseek(f, 0, SEEK_END);
    stringsize = ftell(f);
    rewind(f);

    buffer = malloc(sizeof(char) *(stringsize+1));
    readsize = fread(buffer, sizeof(char), stringsize, f);
    buffer[stringsize] = '\0';
    if (stringsize != readsize){
        fprintf(stderr, "Error reading file into buffer\n");
        free(buffer);
        return 1;
    }
    fclose(f);

    list = malloc(sizeof(struct quote_list));
    int status = maplist(buffer, list);
    if (status!=0){
        fprintf(stderr, "Error mapping json\n");
        return 2;
    }
    free(buffer);

    /* Rest endpoint set-up */
    struct _u_instance ulfius;
    if (ulfius_init_instance(&ulfius, PORT, NULL, NULL) != U_OK){
        fprintf(stderr, "Could not initialize ulfius struct\n");
        exit(2);
    }
    ulfius_add_endpoint_by_val(&ulfius, "GET", "/qotd", NULL, 0, &callback, NULL);

    if (ulfius_start_framework(&ulfius) == U_OK) {
        for (;;)
            pause();
    }
    ulfius_stop_framework(&ulfius);
    ulfius_clean_instance(&ulfius);
    return 0;
}
