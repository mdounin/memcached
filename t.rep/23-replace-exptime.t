#!/usr/bin/perl

use strict;
use Test::More qw(no_plan);
use FindBin qw($Bin);
use lib ("$Bin/lib", "$Bin/../t/lib");
use RepcachedTest;

my($m,$b) = new_repcached();
my($sock_m, $sock_b) = ($m->sock, $b->sock);

my($key, $val) = ('replacetest', 'setval_replace'.$$);
my $vallen = length $val;

print $sock_m "set $key 0 0 $vallen\r\n$val\r\n";
is(scalar <$sock_m>, "STORED\r\n", "stored");
sync_get_is($sock_m, $sock_b, $key, $val);

my $exptime = 3;
my $val2    = 'replaceval'.$$;
my $vallen2 = length $val2;

print $sock_m "replace $key 0 $exptime $vallen2\r\n$val2\r\n";
is(scalar <$sock_m>, "STORED\r\n", "stored");
sync_get_is($sock_m, $sock_b, $key, $val2);
sleep $exptime+1;
sync_get_is($sock_m, $sock_b, $key, undef)

