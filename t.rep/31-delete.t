#!/usr/bin/perl

use strict;
use Test::More qw(no_plan);
use FindBin qw($Bin);
use lib ("$Bin/lib", "$Bin/../t/lib");
use RepcachedTest;

my($m,$b) = new_repcached();
my($sock_m, $sock_b) = ($m->sock, $b->sock);

my($key, $val) = ('deletetest', 'deleteval'.$$);
my $vallen = length $val;

print $sock_m "set $key 0 0 $vallen\r\n$val\r\n";
is(scalar <$sock_m>, "STORED\r\n", "stored");
sync_get_is($sock_m, $sock_b, $key, $val);

print $sock_m "delete $key\r\n";
is(scalar <$sock_m>, "DELETED\r\n", "deleted");
sync_get_is($sock_m, $sock_b, $key, undef)
