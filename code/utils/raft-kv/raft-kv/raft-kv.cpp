#include <stdio.h>
#include <glib.h>
#include <stdint.h>
#include <raft-kv/common/log.h>
#include <raft-kv/server/raft_node.h>

static uint64_t g_id = 0;
static const char *g_cluster = NULL;
static int g_port = 0;
static const char *g_ip = "";
static int g_redis_port = 0;
static const char *g_redis_ip = "";

int main(int argc, char *argv[])
{
  GOptionEntry entries[] = {
      {"id", 'i', 0, G_OPTION_ARG_INT64, &g_id, "node id", NULL},
      {"cluster", 'c', 0, G_OPTION_ARG_STRING, &g_cluster, "comma separated cluster peers", NULL},
      {"port", 'p', 0, G_OPTION_ARG_INT, &g_port, "key-value server port", NULL},
      {"ip", 'I', 0, G_OPTION_ARG_STRING, &g_ip, "key-value server ip", NULL},
      {"redis-port", 'r', 0, G_OPTION_ARG_INT, &g_redis_port, "redis server port", NULL},
      {"redis-ip", 'R', 0, G_OPTION_ARG_STRING, &g_redis_ip, "redis server ip", NULL},
      {NULL}};

  GError *error = NULL;
  GOptionContext *context = g_option_context_new("usage");
  g_option_context_add_main_entries(context, entries, NULL);
  if (!g_option_context_parse(context, &argc, &argv, &error))
  {
    fprintf(stderr, "option parsing failed: %s\n", error->message);
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "id:%lu, port:%d, cluster:%s, ip:%s, redis-port:%d, redis-ip:%s\n", g_id, g_port, g_cluster, g_ip, g_redis_port, g_redis_ip);

  if (g_id == 0 || g_port == 0 || g_cluster == NULL || g_redis_port == 0)
  {
    fprintf(stderr, "id, port, cluster and redis-port are required\n");
    g_option_context_free(context);
    exit(EXIT_FAILURE);
  }

  kv::RaftNode::main(g_id, g_cluster, g_port, g_ip, g_redis_port, g_redis_ip);
  g_option_context_free(context);
}
