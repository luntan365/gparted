/* Copyright (C) 2011 Mike Fleetwood
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "../include/nilfs2.h"

namespace GParted
{

FS nilfs2::get_filesystem_support()
{
	FS fs ;
	fs .filesystem = GParted::FS_NILFS2 ;

	if ( ! Glib::find_program_in_path( "mkfs.nilfs2" ) .empty() )
	{
		fs .create = GParted::FS::EXTERNAL ;
	}

	if ( ! Glib::find_program_in_path( "nilfs-tune" ) .empty() )
	{
		fs .read = GParted::FS::EXTERNAL ;
		fs .read_label = GParted::FS::EXTERNAL ;
		fs .write_label = GParted::FS::EXTERNAL ;
	}

        //Nilfs2 resizing is an online only operation and needs:
        //  mount, umount, nilfs-resize and linux >= 3.0 with nilfs2 support.
        if ( ! Glib::find_program_in_path( "mount" ) .empty()        &&
             ! Glib::find_program_in_path( "umount" ) .empty()       &&
             ! Glib::find_program_in_path( "nilfs-resize" ) .empty() &&
             Utils::kernel_supports_fs( "nilfs2" )                      )
        {
                std::ifstream input( "/proc/version" ) ;
                std::string line ;
                int linux_major_ver = -1 ;
                int linux_minor_ver = -1 ;
                int linux_patch_ver = -1 ;
                if ( input )
                {
                        getline( input, line ) ;
                        sscanf( line .c_str() , "Linux version %d.%d.%d",
			        &linux_major_ver, &linux_minor_ver, &linux_patch_ver ) ;
                        input .close() ;
                }

                if ( linux_major_ver >= 3 )
                {
                        fs .grow = GParted::FS::EXTERNAL ;
                        if ( fs .read ) //needed to determine a minimum file system size.
                                fs .shrink = GParted::FS::EXTERNAL ;
                }
        }

	fs .copy = GParted::FS::GPARTED ;
	fs .move = GParted::FS::GPARTED ;

	//Minimum FS size is 128M+4K using mkfs.nilfs2 defaults
	fs .MIN = 128 * MEBIBYTE + 4 * KIBIBYTE ;

	return fs ;
}

void nilfs2::set_used_sectors( Partition & partition )
{
	if ( ! Utils::execute_command( "nilfs-tune -l " + partition .get_path(), output, error, true ) )
	{
		Glib::ustring::size_type index = output .find( "Free blocks count:" ) ;
		if (   index == Glib::ustring::npos
		    || sscanf( output.substr( index ) .c_str(), "Free blocks count: %Ld", &N ) != 1
		   )
			N = -1 ;

		index = output .find( "Block size:" ) ;
		if (   index == Glib::ustring::npos
		    || sscanf( output.substr( index ) .c_str(), "Block size: %Ld", &S ) != 1
		   )
			S = -1 ;

		if ( N > -1 && S > -1 )
			partition .Set_Unused( Utils::round( N * ( S / double( partition .sector_size) ) ) ) ;
	}
	else
	{
		if ( ! output .empty() )
			partition .messages .push_back( output ) ;

		if ( ! error .empty() )
			partition .messages .push_back( error ) ;
	}
}

void nilfs2::read_label( Partition & partition )
{
	if ( ! Utils::execute_command( "nilfs-tune -l " + partition .get_path(), output, error, true ) )
	{
		Glib::ustring label = Utils::regexp_label( output, "^Filesystem volume name:[\t ]*(.*)$" ) ;
		if ( label != "(none)" )
			partition .label = label;
	}
	else
	{
		if ( ! output .empty() )
			partition .messages .push_back( output ) ;

		if ( ! error .empty() )
			partition .messages .push_back( error ) ;
	}
}

bool nilfs2::write_label( const Partition & partition, OperationDetail & operationdetail )
{
	return ! execute_command( "nilfs-tune -L \"" + partition .label + "\" " + partition .get_path(), operationdetail ) ;
}

bool nilfs2::create( const Partition & new_partition, OperationDetail & operationdetail )
{
	return ! execute_command( "mkfs.nilfs2 -L \"" + new_partition .label + "\" " + new_partition .get_path(), operationdetail ) ;
}

bool nilfs2::resize( const Partition & partition_new, OperationDetail & operationdetail, bool fill_partition )
{
	bool success = true ;

	Glib::ustring mount_point = mk_temp_dir( "", operationdetail ) ;
	if ( mount_point .empty() )
		return false ;

	success &= ! execute_command_timed( "mount -v -t nilfs2 " + partition_new .get_path() + " " + mount_point,
	                                    operationdetail ) ;

	if ( success )
	{
		Glib::ustring cmd = "nilfs-resize -v -y " + partition_new .get_path() ;
		if ( ! fill_partition )
		{
			Glib::ustring size = Utils::num_to_str( floor( Utils::sector_to_unit(
					partition_new .get_sector_length(), partition_new .sector_size, UNIT_KIB ) ) ) + "K" ;
			cmd += " " + size ;
		}
		success &= ! execute_command_timed( cmd, operationdetail ) ;

		success &= ! execute_command_timed( "umount -v " + mount_point, operationdetail ) ;
	}

	rm_temp_dir( mount_point, operationdetail ) ;

	return success ;
}

bool nilfs2::move( const Partition & partition_new
                 , const Partition & partition_old
                 , OperationDetail & operationdetail
                 )
{
	return true ;
}

bool nilfs2::copy( const Glib::ustring & src_part_path
                 , const Glib::ustring & dest_part_path
                 , OperationDetail & operationdetail
                 )
{
	return true ;
}

bool nilfs2::check_repair( const Partition & partition, OperationDetail & operationdetail )
{
	return true ;
}

} //GParted