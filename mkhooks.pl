# Armageddon Recorder - Generate ASM stubs for COM wrappers
# Copyright (C) 2013 Daniel Collins <solemnwarning@solemnwarning.net>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

use strict;
use warnings;

if(scalar @ARGV != 1)
{
	print STDERR "Usage: $0 <class name>\n";
	exit(1);
}

my $class = $ARGV[0];

my $vtable_off = 0;

my @vtable = ();

print "section .text\n";

foreach my $line(<STDIN>)
{
	chomp($line);
	
	if($line eq "")
	{
		next;
	}
	
	my ($method, $hook) = split(/\s+/, $line);
	
	if(defined($hook))
	{
		print "extern _$hook\n";
	}
	
	my $stub = "_$class"."_hook_$method"."_stub";
	
	print "$stub:\n";
	
	# The "this" argument points to an instance within the hook structure
	# which is preceeded by a pointer to the real instance.
	#
	# Set EAX to point to the start of the hook structure.
	#
	print "\tmov eax, dword [esp + 4]\n";
	print "\tsub eax, 4\n";
	
	if(defined($hook))
	{
		# Set "this" argument to the start of hook structure.
		#
		print "\tmov dword [esp + 4], eax\n";
		
		# Jump to hook function.
		#
		print "\tjmp _$hook\n";
	}
	else{
		# Dereference the start of the hook structure to obtain the
		# address of the real instance.
		#
		print "\tmov eax, dword [eax]\n";
		
		# Set the "this" argument to the real instance.
		# 
		print "\tmov dword [esp + 4], eax\n";
		
		# Jump to the corresponding function in the real vtable.
		#
		print "\tmov eax, dword [eax]\n";
		print "\tjmp dword [eax + $vtable_off]\n";
	}
	
	push(@vtable, { method => $method, stub => $stub, offset => $vtable_off });
	
	$vtable_off += 4;
}

print "global _$class"."_hook_init_vtable\n";
print "_$class"."_hook_init_vtable:\n";

print "\tmov eax, dword [esp + 4]\n";
print "\tmov eax, dword [eax + 4]\n";

foreach my $vt(@vtable)
{
	my $stub = $vt->{stub};
	my $off  = $vt->{offset};
	
	print "\tmov dword [eax + $off], $stub\n";
}

print "\tret\n";
