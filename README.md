# Route53 Database access from PostgreSQL

*r53db* is a Foreign Data Wrapper for PostgreSQL that enables you to access AWS Route53 Database Zones like SQL tables.

*r53db* supports reading from (`SELECT`) and writing to (`INSERT`/`UPDATE`/`DELETE`) your Route53 Database for simple records and [ALIAS Targets](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/resource-record-sets-choosing-alias-non-alias.html).

More advanced Route53 Database features like weights, geo-location etc. are not supported... *yet*!

## Demo

For several examples, see [EXAMPLES.md](EXAMPLES.md).

Many folks prefer video, and I like [asciinema](https://asciinema.org), so here we go:

[![asciicast](https://asciinema.org/a/RqO7mJeY7UgTTgJN68i7iTAyP.svg)](https://asciinema.org/a/RqO7mJeY7UgTTgJN68i7iTAyP?speed=2)

## Getting started

### Prerequisites

You'll need:

- An AWS environment (working `~/.aws/config` or assigned Instance Role etc.) with the necessary permissions (see below)
- Golang 1.13+
- AWS Go SDK v2
- PostgreSQL 9.6+, including `pg_config` and development files (headers)

r53db has been successfully tested on:

- FreeBSD 12.1
- Amazon Linux 2
- Debian Buster
- Ubuntu Focal 20.04

Other OS should work as well, if they meet the above list.

You'll be happy to know that it's working on 64-bit ARM as well, so you can use those nice [AWS Graviton](https://aws.amazon.com/ec2/graviton/) instances!

### IAM Permissions for Route53

The following permissions are required:

- [ListHostedZones](https://docs.aws.amazon.com/Route53/latest/APIReference/API_ListHostedZones.html)
- [ListResourceRecordSets](https://docs.aws.amazon.com/Route53/latest/APIReference/API_ListResourceRecordSets.html)
- [ChangeResourceRecordSets](https://docs.aws.amazon.com/Route53/latest/APIReference/API_ChangeResourceRecordSets.html) (you can leave this out if you want to start with read-only access to Route53)


 You can create an IAM Policy using the following document:
 ```
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "route53:ListHostedZones",
                "route53:ListResourceRecordSets",
                "route53:ChangeResourceRecordSets"
            ],
            "Resource": [
                "*"
            ]
        }
    ]
}
 ```

Then simply attach this IAM Policy to your IAM Role / User / Instance Role.

### Installation

When all prerequisites have been met, the general procedure is just:

`make clean all install`.

PostgreSQL installation directory and header files will be located using `pg_config`. If you have multiple PostgreSQL installations, you can point to the right one by setting `$PG_CONFIG`.

After installation, use the `psql` client to connect to your database. Install the extension and hook it up to your Route53 Database:

```
CREATE EXTENSION r53db;
CREATE SERVER route53 FOREIGN DATA WRAPPER r53db;
CREATE SCHEMA route53;
IMPORT FOREIGN SCHEMA dummy FROM SERVER route53 INTO route53;
```

The schema name `route53` is just a suggestion, you can change it if you like (or don't create a separate schema and import into the default  `public` schema).

The `IMPORT FOREIGN SCHEMA` command will create a Foreign Table for each Hosted Zone in your Route53 Database. DNS Names are adjusted to be easy-to-use PostgreSQL table names (e.g. the Hosted Zone `example.com` would become `example_com`).

Run `\dE+ route53.` (note the terminal `.`) afterwards to verify that the foreign tables have been added.

### OS-specific hints

These hints assume that PostgreSQL is already installed. Adjust as necessary.

Afterwards continue with the in-database installation (see above).

###### FreeBSD

```
pkg install go git-lite gettext
go get github.com/aws/aws-sdk-go-v2/aws

git clone https://github.com/apparentorder/r53db.git
cd r53db
make clean all install
```

###### Amazon Linux 2

```
yum install golang git make
#yum install libpq-devel postgresql-server-devel # for AWS-provided PostgreSQL

git clone https://github.com/apparentorder/r53db.git
cd r53db
make clean all install
```

Adjust the installation of `-devel` package names when using the official PostgreSQL repositories.

###### Debian Buster

```
apt-get install make golang-1.14 git
#apt-get install postgresql-server-dev-11 # for Debian-provided PostgreSQL
PATH=/usr/lib/go-1.14/bin:$PATH
go get github.com/aws/aws-sdk-go-v2/aws

git clone https://github.com/apparentorder/r53db.git
cd r53db
make clean all install
```

Adjust the installation of `-dev` package names when using the official PostgreSQL repositories.

###### Ubuntu Focal 20.04

```
apt-get install make golang git
#apt-get install postgresql-11 postgresql-server-dev-11 # for Ubuntu-provided PostgreSQL
go get github.com/aws/aws-sdk-go-v2/aws

git clone https://github.com/apparentorder/r53db.git
cd r53db
make clean all install
```

## Misc

This project was born from a [silly running gag](https://www.lastweekinaws.com/podcast/aws-morning-brief/whiteboard-confessional-route-53-db/).

In that spirit, this project was meant as a gag as well. Unfortunately, I've severely underestimated how much time I'd have to invest if I make bad design decisions (for example, rolling my own in Golang, instead of doing the Right Thing and using [Multicorn](https://multicorn.org)). So now we're here and it feels more like a project than like a gag.

And who knows -- I can actually imagine this being useful in some contexts, like the use-case described in that link. After all, if you have a lot of configuration data in PostgreSQL anyway, it's a small jump from there to maintaining DNS data as well.

Either way, let me know if you find this useful at all!

### Prior Art

Similar projects that I'm aware of:

- https://github.com/craftyphotons/ten34

I'm sure there's more.

## Contact

For suggestions, bugs, pull requests etc. please use [Github](https://github.com/apparentorder/r53db).

For everything else: I'm trying to get used to Twitter as [@apparentorder](https://twitter.com/apparentorder). Or try legacy message delivery using apparentorder@neveragain.de. Also I'm old enough to use IRC -- I'm hiding somewhere in `##aws` (Freenode).

