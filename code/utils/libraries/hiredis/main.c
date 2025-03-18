#include <stdio.h>
#include <stdlib.h>
#include <hiredis/hiredis.h>

int main(void)
{
    // Connect to Redis server at 127.0.0.1:6379
    redisContext *c = redisConnect("127.0.0.2", 10000);
    if (c == NULL || c->err)
    {
        if (c)
        {
            fprintf(stderr, "Connection error: %s\n", c->errstr);
            redisFree(c);
        }
        else
        {
            fprintf(stderr, "Connection error: can't allocate redis context\n");
        }
        exit(EXIT_FAILURE);
    }

    // Set key "foo" to "hello world"
    redisReply *reply = redisCommand(c, "SET %s %s", "foo", "hello world");
    if (reply == NULL)
    {
        fprintf(stderr, "SET command failed\n");
        redisFree(c);
        exit(EXIT_FAILURE);
    }
    printf("SET: %s\n", reply->str);
    freeReplyObject(reply);

    // Get the value of key "foo"
    reply = redisCommand(c, "GET %s", "foo");
    if (reply == NULL)
    {
        fprintf(stderr, "GET command failed\n");
        redisFree(c);
        exit(EXIT_FAILURE);
    }
    printf("GET foo: %s\n", reply->str);
    freeReplyObject(reply);

    // Clean up and disconnect
    redisFree(c);
    return 0;
}
