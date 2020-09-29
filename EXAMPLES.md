# More examples of r53db usage

### Add IPv6 to ALIAS Targets

As you are a good net citizen, you're keen on adding IPv6 support to your public-facing services!
You gotta start somewhere, you know. And it's the *only* place where AWS *lets* you start.

Assume we have some DNS names pointing to some ALIAS targets:
```
postgres=# select name, type, at_dns_name from route53.cmdu_de where at_dns_name is not null;
       name       | type |                     at_dns_name                      
------------------+------+------------------------------------------------------
 cmdu.de.         | A    | s3-website.eu-central-1.amazonaws.com.
 blergh.cmdu.de.  | A    | d-pr5rizm6m3.execute-api.eu-central-1.amazonaws.com.
 www.cmdu.de.     | A    | s3-website.eu-central-1.amazonaws.com.
 wwwtest.cmdu.de. | A    | s3-website.eu-central-1.amazonaws.com.
(4 rows)
```

Now let's do a clever `INSERT INTO ... SELECT` to add our IPv6 records:
```
postgres=# insert into route53.cmdu_de (name, type, at_dns_name, at_hosted_zone_id)
postgres-# select name, 'AAAA', at_dns_name, at_hosted_zone_id
postgres-# from route53.cmdu_de 
postgres-# where at_dns_name is not null AND type='A'
postgres-# returning name, type, at_dns_name;

       name       | type |                     at_dns_name                      
------------------+------+------------------------------------------------------
 cmdu.de.         | AAAA | s3-website.eu-central-1.amazonaws.com.
 blergh.cmdu.de.  | AAAA | d-pr5rizm6m3.execute-api.eu-central-1.amazonaws.com.
 www.cmdu.de.     | AAAA | s3-website.eu-central-1.amazonaws.com.
 wwwtest.cmdu.de. | AAAA | s3-website.eu-central-1.amazonaws.com.
(4 rows)
```

### Comparing different zones

Let's create an example where we have a few `A` entries in different Hosted Zones:
```
postgres=# insert into route53.cmdu_de (name, type, data)
postgres-# values ('foo.cmdu.de', 'A', '10.53.0.42'), ('bar.cmdu.de', 'A', '10.53.0.23');
INSERT 0 2

postgres=# insert into route53.route53_db (name, type, data)
postgres-# values ('kallisti.route53.db.', 'A', '10.53.0.23'), ('eris.route53.db', 'A', '10.53.0.99');
INSERT 0 2
```

Now if you ever wanted to figure out which records in different zones point to the same IP address,
you can just use standard SQL to join them:
```
postgres=# select zone_a.name, zone_a.type, zone_a.data, zone_b.name
postgres-# from route53.cmdu_de zone_a
postgres-# inner join route53.route53_db zone_b using (data);

     name     | type |    data    |         name         
--------------+------+------------+----------------------
 bar.cmdu.de. | A    | 10.53.0.23 | kallisti.route53.db.
(1 row)
```

### Using inet operators

PostgreSQL has a data type for IP addresses, `inet`. You can use it, for example, to select
addresses that are contained in specific CIDR blocks:
```
postgres=# select name, type, data from route53.cmdu_de
postgres-# where type='A' and data::inet <<= '10.192.0.0/19';
     name     | type |    data     
--------------+------+-------------
 foo.cmdu.de. | A    | 10.192.0.3
 foo.cmdu.de. | A    | 10.192.10.3
(2 rows)
```

Of course it knows about IPv6, too!
```
postgres=# select name, type, data from route53.cmdu_de
postgres-# where type='AAAA' and data::inet <<= '2001:db8::/64';
     name     | type |      data       
--------------+------+-----------------
 bar.cmdu.de. | AAAA | 2001:db8::53
 bar.cmdu.de. | AAAA | 2001:db8::53:53
(2 rows)
```

### Updating ALIAS Targets

You can `UPDATE` ALIAS Targets just as well, either to a completely different `hosted_zone_id`, or
toggle individual options:
```
postgres=# select * from route53.cmdu_de where name ~ 'blergh' and type='A';
      name       | type | ttl | data |                     at_dns_name                      | at_hosted_zone_id | at_evaluate_target_health 
-----------------+------+-----+------+------------------------------------------------------+-------------------+---------------------------
 blergh.cmdu.de. | A    |     |      | d-pr5rizm6m3.execute-api.eu-central-1.amazonaws.com. | Z1U9ULNL0V5AJ3    | f
(1 row)

postgres=# update route53.cmdu_de set at_evaluate_target_health=true where at_hosted_zone_id = 'Z1U9ULNL0V5AJ3';
UPDATE 1
```

Of course, `at_evaluate_target_health` is the only option currently supported... but that might change, of course.

### Inserting TXT records

Just like in BIND Zone Files, TXT records have to be in double-quotes, which makes it a bit annoying to type
in PostgreSQL:
```
postgres=# insert into route53.cmdu_de (name, type, data)
postgres-# values
postgres-# ('foo.cmdu.de', 'TXT', '"foobar"'),
postgres-# ('bar.cmdu.de', 'TXT', format('"test: %s"', random()))
postgres-# returning name, type, ttl, data;

    name     | type | ttl |            data            
-------------+------+-----+----------------------------
 foo.cmdu.de | TXT  | 300 | "foobar"
 bar.cmdu.de | TXT  | 300 | "test: 0.4357084824032178"
(2 rows)
```

## Other examples?

Do you have other nice examples? Let me know!

