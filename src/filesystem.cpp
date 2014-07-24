#include "filesystem.hpp"
#include "Sequence.hpp"
#include "Folder.hpp"
#include "File.hpp"
#include "utils.hpp"

#include "detail/analyze.hpp"
#include "detail/FileNumbers.hpp"
#include "detail/FileStrings.hpp"
#include "commonDefinitions.hpp"

#include <boost/regex.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <set>

#include <sys/types.h>
#include <sys/stat.h>


namespace sequenceParser {

using detail::FileNumbers;
using detail::FileStrings;
using detail::SeqIdHash;
namespace bfs = boost::filesystem;


std::string Item::getAbsFilepath() const
{
	return _path.string();
}

std::string Item::getFilename() const
{
	return _path.filename().string();
}

std::string Item::getFolder() const
{
	return _path.parent_path().string();
}

std::string Item::getAbsoluteFirstFilename() const
{
	if( getType() == eTypeSequence )
		return getSequence().getAbsoluteFirstFilename();
	return getAbsFilepath();
}

std::string Item::getFirstFilename() const
{
	if( getType() == eTypeSequence )
		return getSequence().getFirstFilename();
	return getFilename();
}


ItemStat::ItemStat( const Item& item, const bool approximative )
{
	switch(item.getType())
	{
		case eTypeFolder:
		case eTypeFile:
		{
			statFile(item);
			break;
		}
		case eTypeSequence:
		{
			statSequence( item, approximative );
			break;
		}
		case eTypeUndefined:
			BOOST_ASSERT(false);
	}
}

void ItemStat::statFile( const Item& item )
{
	using namespace boost::filesystem;
	boost::system::error_code errorCode;

	_fullNbHardLinks = _nbHardLinks = hard_link_count( item.getPath(), errorCode );
	_size = file_size(item.getPath(), errorCode);
	_modificationTime = last_write_time(item.getPath(), errorCode);

#ifdef UNIX
	struct stat statInfos;
	lstat( item.getAbsFilepath().c_str(), &statInfos );
	_deviceId = statInfos.st_dev;
	_inodeId = statInfos.st_ino;
	_userId = statInfos.st_uid;
	_groupId = statInfos.st_gid;
	_accessTime = statInfos.st_atime;
	_creationTime = statInfos.st_ctime;
	// size on hard-drive (takes hardlinks into account)
	_sizeOnDisk = (statInfos.st_blocks / _nbHardLinks) * 512;
#else
	_deviceId = 0;
	_inodeId = 0;
	_userId = 0;
	_groupId = 0;
	_accessTime = 0;
	_creationTime = 0;
	_sizeOnDisk = 0;
#endif

	// size (takes hardlinks into account)
	_realSize = _size / _nbHardLinks;
}

void ItemStat::statSequence( const Item& item, const bool approximative )
{
	using namespace boost::filesystem;
	using namespace sequenceParser;
	boost::system::error_code errorCode;

#ifdef UNIX
	struct stat statInfos;
	lstat( item.getAbsoluteFirstFilename().c_str(), &statInfos );
	_deviceId = statInfos.st_dev;
	_inodeId = statInfos.st_ino;
	_userId = statInfos.st_uid;
	_groupId = statInfos.st_gid;
	_accessTime = statInfos.st_atime;
#else
	_deviceId = 0;
	_inodeId = 0;
	_userId = 0;
	_groupId = 0;
	_accessTime = 0;
#endif

	_modificationTime = 0;
	_fullNbHardLinks = 0;
	_size = 0;
	_realSize = 0;
	_sizeOnDisk = 0;
	_creationTime = 0;

	const Sequence& seq = item.getSequence();
	for( Time t = seq.getFirstTime(); t <= seq.getLastTime(); ++t )
	{
		boost::filesystem::path filepath = seq.getAbsoluteFilenameAt(t);

		long long fileNbHardLinks = hard_link_count( filepath, errorCode );
		long long fileSize = file_size(filepath, errorCode);
		long long fileModificationTime = last_write_time(filepath, errorCode);

		if( fileModificationTime > _modificationTime )
			_modificationTime = fileModificationTime;

		_fullNbHardLinks += fileNbHardLinks;
		_size += fileSize;
		// realSize takes hardlinks into account
		long double fileRealSize = fileSize / fileNbHardLinks;
		_realSize += fileRealSize;

		#ifdef UNIX
			struct stat statInfos;
			lstat( filepath.c_str(), &statInfos );
			if( _creationTime == 0 || _creationTime > statInfos.st_ctime )
				_creationTime = statInfos.st_ctime;
			// size on hard-drive (takes hardlinks into account)
			_sizeOnDisk += (statInfos.st_blocks / fileNbHardLinks) * 512;
		#else
			_creationTime = 0;
			_sizeOnDisk = 0;
		#endif
	}

	_nbHardLinks = _fullNbHardLinks / (double)(seq.getLastTime() - seq.getFirstTime() + 1);
}


std::vector<Item> browse(
		const std::string& dir,
		const std::vector<std::string>& filters,
		const EDetection detectOptions,
		const EDisplay displayOptions )
{
	std::vector<Item> output;
	std::string tmpDir( dir );
	std::vector<std::string> tmpFilters( filters );
	std::string filename;

	if( ! detectDirectoryInResearch( tmpDir, tmpFilters, filename ) )
		return output;

	const std::vector<boost::regex> reFilters = convertFilterToRegex( tmpFilters, detectOptions );

	// variables for sequence detection
	typedef boost::unordered_map<FileStrings, std::vector<FileNumbers>, SeqIdHash> SeqIdMap;
	bfs::path directory( tmpDir );
	SeqIdMap sequences;
	FileStrings tmpStringParts; // an object uniquely identify a sequence
	FileNumbers tmpNumberParts; // the vector of numbers inside one filename

	// for all files in the directory
	bfs::directory_iterator itEnd;
	for( bfs::directory_iterator iter( directory ); iter != itEnd; ++iter )
	{
		// clear previous infos
		tmpStringParts.clear();
		tmpNumberParts.clear(); // (clear but don't realloc the vector inside)

		if( filepathRespectsAllFilters( iter->path(), reFilters, filename, detectOptions ) )
		{
			// it's a file or a file of a sequence
			if( filenameRespectsFilters( iter->path().filename().string(), reFilters ) ) // filtering of entries with filters strings
			{
				// if at least one number detected
				if( decomposeFilename( iter->path().filename().string(), tmpStringParts, tmpNumberParts, detectOptions ) )
				{
					const SeqIdMap::iterator it( sequences.find( tmpStringParts ) );
					if( it != sequences.end() ) // is already in map
					{
						// append the vector of numbers
						sequences.at( tmpStringParts ).push_back( tmpNumberParts );
					}
					else
					{
						// create an entry in the map
						std::vector<FileNumbers> li;
						li.push_back( tmpNumberParts );
						sequences.insert( SeqIdMap::value_type( tmpStringParts, li ) );
					}
				}
				else
				{
					EType type = bfs::is_directory(iter->path()) ? eTypeFolder : eTypeFile;
					output.push_back( Item( type, iter->path().filename().string(), directory.string() ) );
				}
			}
		}
	}

	// add sequences in the output vector
	BOOST_FOREACH( SeqIdMap::value_type & p, sequences )
	{
		const std::vector<Sequence> ss = buildSequences( directory, p.first, p.second, detectOptions, displayOptions );

		BOOST_FOREACH( const std::vector<Sequence>::value_type & s, ss )
		{
			// don't detect sequence of directories
			if( bfs::is_directory( s.getAbsoluteFirstFilename() ) )
			{
				// TODO: declare folders individually
			}
			else
			{
				// if it's a sequence of 1 file, it could be considered as a sequence or as a single file
				if( (detectOptions & eDetectionSequenceNeedAtLeastTwoFiles) && (s.getNbFiles() == 1) )
				{
					output.push_back( Item( eTypeFile, s.getFirstFilename(), directory.string() ) );
				}
				else
				{
					output.push_back( Item( Sequence( directory, s, displayOptions ), directory.string() ) );
				}
			}
		}
	}
	return output;
}


}