#include <boost/test/unit_test.hpp>
#include <envire_maps/LocalMap.hpp>

#include <iostream>

using namespace envire::maps;

BOOST_AUTO_TEST_CASE(test_local_map_default_constuctor)
{
    // default
    LocalMap *map_1 = new LocalMap();

    BOOST_CHECK_EQUAL(map_1->getId(), std::string(""));
    BOOST_CHECK_EQUAL(map_1->getOffset().matrix().isApprox(base::Transform3d::Identity().matrix()), true); 

    delete map_1;
}

BOOST_AUTO_TEST_CASE(test_local_map_constuctor)
{
    // fix parameter
    boost::shared_ptr<LocalMapData> data(new LocalMapData());
    data->id = "local_map_data";
    data->offset = base::Transform3d::Identity();

    Eigen::Vector3d translation = Eigen::Vector3d::Random(3);
    data->offset.translate(translation);

    // both map should have same parameter
    LocalMap *map_2 = new LocalMap(data);

    BOOST_CHECK_EQUAL(map_2->getId(), data->id);
    BOOST_CHECK_EQUAL(map_2->getOffset().matrix().isApprox(data->offset.matrix()), true); 

    LocalMap *map_3 = new LocalMap(data);

    BOOST_CHECK_EQUAL(map_3->getId(), data->id);
    BOOST_CHECK_EQUAL(map_3->getOffset().matrix().isApprox(data->offset.matrix()), true); 

    // changes should be applied to the both maps
    boost::shared_ptr<LocalMapData> data_new(new LocalMapData());
    data_new->id = "changed";
    data_new->offset = base::Transform3d::Identity();  
    data_new->offset.translate(Eigen::Vector3d::Random(3));  

    map_2->getId() = data_new->id;
    map_3->getOffset() = data_new->offset;

    BOOST_CHECK_EQUAL(map_2->getId(), data_new->id);
    BOOST_CHECK_EQUAL(map_3->getOffset().matrix().isApprox(data_new->offset.matrix()), true);  

    BOOST_CHECK_EQUAL(map_2->getId(), map_3->getId());
    BOOST_CHECK_EQUAL(map_2->getOffset().matrix().isApprox(map_3->getOffset().matrix()), true);            
}

BOOST_AUTO_TEST_CASE(test_local_map_copy)
{
    // fix parameter
    std::string first_name("local_map_data");
    boost::shared_ptr<LocalMapData> data(new LocalMapData());
    data->id = first_name;
    data->offset = base::Transform3d::Identity();

    Eigen::Vector3d translation = Eigen::Vector3d::Random(3);
    data->offset.translate(translation);    

    // both map should have same parameter
    LocalMap *map_2 = new LocalMap(data);

    LocalMap *map_3 = new LocalMap(*map_2);  

    BOOST_CHECK_EQUAL(map_2->getId(), map_3->getId());
    BOOST_CHECK_EQUAL(map_2->getOffset().matrix().isApprox(map_3->getOffset().matrix()), true);   

    // change the parameter of one map should not affect the other map 
    boost::shared_ptr<LocalMapData> data_new(new LocalMapData());
    data_new->id = "changed";
    data_new->offset = base::Transform3d::Identity();  

    map_2->getId() = data_new->id;
    map_2->getOffset() = data_new->offset;

    BOOST_CHECK_EQUAL(map_2->getId(), data_new->id);     
    BOOST_CHECK_EQUAL(map_3->getId(), first_name);
}