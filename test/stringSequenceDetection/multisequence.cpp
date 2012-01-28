#include <Detector.hpp>

#include <boost/assign/std/vector.hpp>

#include <boost/test/unit_test.hpp>
using namespace boost::unit_test;

BOOST_AUTO_TEST_SUITE( MutiSequenceDetection )

BOOST_AUTO_TEST_CASE( SimpleMultiSequence )
{
	sequenceParser::Detector detector;
	std::list<boost::shared_ptr<sequenceParser::Sequence > > listSequence;

	std::vector<boost::filesystem::path> paths;

	boost::assign::push_back( paths )
		( "aaa/bbb/a1b2c1.j2c" )
		( "aaa/bbb/a1b2c2.j2c" )
		( "aaa/bbb/a1b2c3.j2c" )

		( "aaa/bbb/a1b3c6.j2c" )
		( "aaa/bbb/a1b3c2.j2c" )
		( "aaa/bbb/a1b3c3.j2c" )

		( "aaa/bbb/a1b9c6.j2c" )
		( "aaa/bbb/a1b9c2.j2c" )
		;

	listSequence = detector.sequenceFromFilenameList( paths );

	BOOST_CHECK( listSequence.size() == 3 );
}

BOOST_AUTO_TEST_CASE( SimpleMultiSequenceMultiLevel )
{
	sequenceParser::Detector detector;
	std::list<boost::shared_ptr<sequenceParser::Sequence > > listSequence;

	std::vector<boost::filesystem::path> paths;

	boost::assign::push_back( paths )
		( "aaa/bbb/a1b2c1.j2c" )
		( "aaa/bbb/a1b2c2.j2c" )
		( "aaa/bbb/a1b2c3.j2c" )

		( "aaa/bbb/a1b3c4.j2c" )
		( "aaa/bbb/a1b4c4.j2c" )
		( "aaa/bbb/a1b5c4.j2c" )
		;

	listSequence = detector.sequenceFromFilenameList( paths );

	std::cout << "AA: " << listSequence.size() << std::endl;
	BOOST_CHECK( listSequence.size() == 2 );
}


BOOST_AUTO_TEST_SUITE_END()