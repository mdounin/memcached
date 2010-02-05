#!/usr/bin/perl

use strict;
use Test::More qw(no_plan);
use FindBin qw($Bin);
use lib ("$Bin/lib", "$Bin/../t/lib");
use RepcachedTest;

my($m,$b) = new_repcached();
my($sock_m, $sock_b) = ($m->sock, $b->sock);

my $key = 'append-prepend-test';
my($prepend, $base, $append) = ('PPP', 'BASE', 'AAA'); #val

print $sock_m "append $key 0 0 ".length($append)."\r\n$append\r\n";
is(scalar <$sock_m>, "NOT_STORED\r\n", "not stored");
sync_get_is($sock_m, $sock_b, $key, undef);

print $sock_m "prepend $key 0 0 ".length($prepend)."\r\n$prepend\r\n";
is(scalar <$sock_m>, "NOT_STORED\r\n", "not stored");
sync_get_is($sock_m, $sock_b, $key, undef);


print $sock_m "set $key 0 0 ".length($base)."\r\n$base\r\n";
is(scalar <$sock_m>, "STORED\r\n", "stored");


print $sock_m "append $key 0 0 ".length($append)."\r\n$append\r\n";
is(scalar <$sock_m>, "STORED\r\n", "stored");
sync_get_is($sock_m, $sock_b, $key, $base.$append);

print $sock_m "prepend $key 0 0 ".length($prepend)."\r\n$prepend\r\n";
is(scalar <$sock_m>, "STORED\r\n", "stored");
sync_get_is($sock_m, $sock_b, $key, $prepend.$base.$append);

