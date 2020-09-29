# Route53 Database access from PostgreSQL

*r53db* is a [Foreign Data Wrapper](https://wiki.postgresql.org/wiki/Foreign_data_wrappers) for PostgreSQL.

It provides read and write access to the AWS Route53 Database API via standard SQL semantics.

### Demo

For several examples, see [EXAMPLES.md](EXAMPLES.md).

Many folks like video, and I like [asciinema](https://asciinema.org), so here we go:

<!--
<script id="asciicast-e50O6FMUkU2pmZu30xMHuBRsJ" src="https://asciinema.org/a/e50O6FMUkU2pmZu30xMHuBRsJ.js" data-speed="5" data-cols="80" data-rows="30" async></script>
-->

### Features

r53db supports reading (`SELECT`) and writing (`INSERT`/`UPDATE`/`DELETE`) to your Route53 Database for:

- Simple records
- [ALIAS targets](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/resource-record-sets-choosing-alias-non-alias.html) (including Health Checks)

More advanced features like weights, geo-location etc. are not supported... *yet*.

### Prerequisites

You'll need:

- AWS environment (a working `~/.aws/config`, an assigned Instance Role etc.) with the necessary permissions (see below)
- Golang 1.13+
- AWS Go SDK v2
- PostgreSQL 9.6+, including `pg_config` and development files (headers)

r53db has been successfully tested on:

- FreeBSD 12.1
- Amazon Linux 2
- Debian Buster
- Ubuntu Focal 20.04
- PostgreSQL Versions 9.6 up to 13

Other OS might work as well.

##### IAM Permissions for Route53

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

The `IMPORT FOREIGN SCHEMA` command will `CREATE FOREIGN TABLE` for each Hosted Zone in your Route53 Database. DNS Names are adjusted to be easy-to-use PostgreSQL table names (e.g. the Hosted Zone `example.com` would become `example_com`).

Run `\dE+ route53.` (note the terminal `.`) afterwards to verify that the foreign tables have been added.

##### OS-specific instructions

These instructions assume that PostgreSQL is already installed. Adjust as necessary.

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

