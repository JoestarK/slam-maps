//
// Copyright (c) 2015-2017, Deutsches Forschungszentrum für Künstliche Intelligenz GmbH.
// Copyright (c) 2015-2017, University of Bremen
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
#pragma once

#include <list>
#include "MultiLevelGridMap.hpp"
#include "SurfacePatches.hpp"
#include <map>
#include <type_traits>

namespace maps { namespace grid
{

    template <class T>
    class TraversabilityMap3d;
    
    class TraversabilityNodeBase
    {
    public:
        enum TYPE
        {
            OBSTACLE,
            TRAVERSABLE,
            UNKNOWN,
            HOLE,
            UNSET,
            FRONTIER, //a node that is traversable but is on the border to missing map information
        };

        TraversabilityNodeBase(float height, const Index &idx);

        float getHeight() const;
        float getMin() const;
        float getMax() const;
        void setHeight(float newHeight);

        //! Given a grid resolution, compute the 3d position of this node
        Eigen::Vector3d getVec3(double grid_res) const;

        /**
         * Returns the index of this cell
         * */
        const Index &getIndex() const;

        void addConnection(TraversabilityNodeBase *node);
        
        const std::vector<TraversabilityNodeBase *> &getConnections() const;
        
        TraversabilityNodeBase *getConnectedNode(const Index &toIdx) const;
        
        bool operator<(const TraversabilityNodeBase& other) const;
        
        void eachConnectedNode(std::function<void (const TraversabilityNodeBase *n, bool &explandNode, bool &stop)> f) const;
        void eachConnectedNode(std::function<void (TraversabilityNodeBase *n, bool &explandNode, bool &stop)> f);
        
        template <class T>
        Eigen::Vector3d getPosition(const T &map) const
        {
            Eigen::Vector3d pos;
            map.fromGrid(idx, pos, height, false);
            return pos;
        }

        bool isExpanded() const;
        void setExpanded();
        void setNotExpanded();
        
        void setType(TYPE t);
        TYPE getType() const;
        
    protected:
        TraversabilityNodeBase() {};
        
        /** Grants access to boost serialization */
        friend class boost::serialization::access;

        template <class X>
        friend class TraversabilityMap3d;
        
        template<class Archive>
        void serialize(Archive & ar, const unsigned int version)
        {
            //we don't save the connections here, as this 
            //would lead to a stack corruption caused by
            //to much recursive calls. The connections are
            //Saved and set in the map
            
            ar & height;
            ar & idx;
            ar & type;
            ar & mIsExpanded;
        }

        std::vector<TraversabilityNodeBase *> connections;
        float height;
        ::maps::grid::Index idx;
        enum TYPE type;
        ///determines whether this node is a candidate or a final node
        bool mIsExpanded;
    };

    template <class T>
    class TraversabilityNode : public TraversabilityNodeBase
    {
    protected:
        /** Grants access to boost serialization */
        friend class boost::serialization::access;
        
        template<class Archive>
        void serialize(Archive & ar, const unsigned int version)
        {
            ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(TraversabilityNodeBase);
            ar & userData;
        }

        T userData;

        //needed for boost serialization
        TraversabilityNode() : TraversabilityNodeBase()
        {
        };
    public:
        TraversabilityNode(float height, const Index& idx) : 
            TraversabilityNodeBase(height, idx)
        {
        };
        
        T &getUserData()
        {
            return userData;
        };

        const T &getUserData() const
        {
            return userData;
        };
        
        TraversabilityNode<T> *getConnectedNode(const Index &toIdx) const
        {
            return static_cast<TraversabilityNode<T> *>(TraversabilityNodeBase::getConnectedNode(toIdx));
        }
    };

    template <class T>
    class TraversabilityMap3d : public ::maps::grid::MultiLevelGridMap<T>
    {
        bool ownsNodePointers;
    public:
        TraversabilityMap3d() : ownsNodePointers(true) {};
        
        TraversabilityMap3d(const Vector2ui &num_cells,
                    const Eigen::Vector2d &resolution,
                    const boost::shared_ptr<LocalMapData> &data) : MultiLevelGridMap<T>(num_cells, resolution, data),
                    ownsNodePointers(true)
        {}

        ~TraversabilityMap3d()
        {
            clear();
        }
        
        Eigen::Vector3f getNodePosition(const TraversabilityNodeBase *node) const
        {
            Eigen::Vector3d pos;
            if(!this->fromGrid(node->getIndex(), pos))
                throw std::runtime_error("Internal error, could not calculate position from index");
            
            pos.z() += node->getHeight();
            
            return pos.cast<float>();
        }
        
        TraversabilityMap3d<T> &operator=(const TraversabilityMap3d<T> &other)
        {
            clear();
            this->setResolution(other.getResolution());
            this->resize(other.getNumCells());
            this->getLocalFrame() = other.getLocalFrame();

            doDeepCopy(other, *this);
            
            return *this;
        }
        
        template <class X>
        TraversabilityMap3d<X> cast() const
        {
            static_cast<X>((T)(nullptr));
            
            TraversabilityMap3d<X> out(this->getNumCells(), this->getResolution(), this->getLocalMapData());
            
            for(size_t y = 0 ; y < this->getNumCells().y(); y++)
            {
                for(size_t x = 0 ; x < this->getNumCells().x(); x++)
                {
                    Index idx(x, y);
                    for(auto &p : this->at(idx))
                    {
                        out.at(idx).insert(p);
                    }
                }
            }
            
            out.setMapOwnsNodePointers(false);
            return out;

        }
        
        /** @return the node closest to pos.z() of all nodes at (pos.x(), pos.y()).
         *          nullptr is returned if there are no nodes at (pos.x(), pos.y()).
         * @param pos The position in world coordinates.*/
        TraversabilityNodeBase* getClosestNode(const base::Vector3d& pos) const
        {
            Index idx;
            if(::maps::grid::MultiLevelGridMap<T>::toGrid(pos, idx))
            {
                double minDist = std::numeric_limits< double >::max();
                TraversabilityNodeBase *node = nullptr;
                for(TraversabilityNodeBase *currNode : ::maps::grid::MultiLevelGridMap<T>::at(idx))
                {
                    double curDist = fabs(currNode->getHeight() - pos.z());
                    if(curDist < minDist)
                    {
                        minDist = curDist;
                        node = currNode;
                    }
                }
                return node;
            }

            return nullptr;
        }
        
        void clear()
        {
            if(ownsNodePointers)
            {
                for(LevelList<T> &l : *this)
                {
                    for(T &n : l)
                    {
                        delete n;
                    }
                    
                    l.clear();
                }
            }
            
            ::maps::grid::MultiLevelGridMap<T>::clear();
        }

        void setMapOwnsNodePointers(bool owns)
        {
            ownsNodePointers = owns;
        }
    protected:
        /** Grants access to boost serialization */
        friend class boost::serialization::access;

        void doDeepCopy(const TraversabilityMap3d<T> &in, TraversabilityMap3d<T> &out)
        {
            std::map<T, T> inToOut;
            for(const LevelList<T> &l : in)
            {
                for(const T &n : l)
                {
                    T newNode = new typename std::remove_pointer<T>::type(*n);
                    newNode->connections.clear();
                    out.at(n->getIndex()).insert(newNode);
                    
                    inToOut[n] = newNode;
                }
            }

            for(const LevelList<T> &l : in)
            {
                for(const T &n : l)
                {
                    T newNode = inToOut[n];

                    for(const T &neighbour: n->getConnections())
                    {
                        newNode->connections.push_back(inToOut[neighbour]);
                    }
                }
            }

            
        }
        
        struct SerializationHelper
        {
            T node;
            std::vector<TraversabilityNodeBase *> connections;
            
            /** Serializes the members of this class*/
            template<class Archive>
            void serialize(Archive & ar, const unsigned int version)
            {
                ar & node;
                ar & connections;
            }
        };
        
        /** Serializes the members of this class*/
        BOOST_SERIALIZATION_SPLIT_MEMBER()
        template<class Archive>
        void load(Archive &ar, const unsigned int version)
        {
            //load back all pointers
            ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(::maps::grid::MultiLevelGridMap<T>);

            //load nr of contained values
            uint64_t count;
            loadSizeValue(ar, count);
            
            for(size_t i=0; i<count; ++i)
            {
                SerializationHelper helper;
                ar >> helper;

                //set back the connections. As all pointers have been loaded before, this should
                //also apply to all objects in the MultiLevelGridMap.
                for(TraversabilityNodeBase *n : helper.connections)
                {
                    helper.node->addConnection(n);
                }
            }
        }
        template<class Archive>
        void save(Archive& ar, const unsigned int version) const
        {
            //first save all pointers without connections
            ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(::maps::grid::MultiLevelGridMap<T>);

            //determine nr of nodes
            uint64_t size = 0;
            for(const maps::grid::LevelList<T> &ll : *this )
            {
                size += ll.size();
            }
            saveSizeValue(ar, size);

            //save all connections together with the coresponding pointer
            for(const maps::grid::LevelList<T> &ll : *this )
            {
                for(const T &node: ll)
                {
                    SerializationHelper helper;
                    helper.node = node;
                    helper.connections = node->getConnections();
                    
                    ar & helper;
                }
            }
        }
    };

    
    typedef TraversabilityMap3d<TraversabilityNodeBase *> TraversabilityBaseMap3d;
}}

