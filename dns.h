#ifndef R53DB_DNS_H
#define R53DB_DNS_H

typedef struct {
	char *name;
	char *type;
	uint32_t ttl;
	char *data;
	char *at_dns_name;
	char *at_hosted_zone_id;
	bool at_evaluate_target_health;
} r53dbDNSRR;

typedef struct {
	char *id;
	char *name;
	char *table_name;
} r53dbZone;

#endif // R53DB_DNS_H
