//------------------------------------------------------------------------------
// File: RadosMap.hh
// Author: Elvin Sindrilaru <esindril@cern.ch>
//------------------------------------------------------------------------------

/*******************************************************************************
 * CephVectMap                                                                 *
 * Copyright (C) 2015 CERN/Switzerland                                         *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation, either version 3 of the License, or           *
 * (at your option) any later version.                                         *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program. If not, see <http://www.gnu.org/licenses/>.        *
 ******************************************************************************/

#ifndef __RADOSMAP_HH__
#define __RADOSMAP_HH__

#include <map>
#include <cerrno>
#include <climits>
#include <string>
#include <sstream>
#include <cstdio>
#include <utility>
#include <cstdint>
#include <cstddef>
#include <typeinfo>
#include <chrono>
#include <thread>
#include <rados/librados.hpp>
#include "RadosException.hh"

namespace rados {

  //----------------------------------------------------------------------------
  //! Rados map class which is backed-up by a object
  //----------------------------------------------------------------------------
  template<typename K, typename V>
  class map
  {
    typedef typename std::map<K, V>::iterator maplocal_iterator_t;

  public:

    //--------------------------------------------------------------------------
    //! Constructor
    //!
    //! @param rados_cluster Rados cluster obj
    //! @param pool_name name of the pool the map obj will be in
    //! @param name name of the map
    //! @param cookie application identifier
    //! @param persist_obj persist backend obj. (delete or not obj. holding the map)
    //!
    //! TODO: add param weak consistency when all operations are done async.
    //!
    //--------------------------------------------------------------------------
    map(librados::Rados& rados_cluster,
        const std::string& pool_name,
        const std::string& name,
        const std::string& cookie,
        bool persist_obj = true) noexcept(false);


    //--------------------------------------------------------------------------
    //! Copy constructor - disabled
    //--------------------------------------------------------------------------
    map(const map& other) = delete;

    //--------------------------------------------------------------------------
    //! Copy constructor - disabled
    //--------------------------------------------------------------------------
    map& operator=(const map& other) = delete;

    //--------------------------------------------------------------------------
    //! Destructor
    //--------------------------------------------------------------------------
    virtual ~map();

    //--------------------------------------------------------------------------
    //! Insert new value
    //!
    //! @param key key
    //! @param value value
    //!
    //! @return pair with the iterator pointing to the newly added element and
    //!         status of the insert - true if element inserted, otherwise
    //!         false
    //--------------------------------------------------------------------------
    std::pair<maplocal_iterator_t, bool>
    insert(K key, V value);

    //--------------------------------------------------------------------------
    //! Erase key from map
    //!
    //! @param key key to be erased from the map
    //--------------------------------------------------------------------------
    void erase(K key);

    //--------------------------------------------------------------------------
    //! Erase entry pointed by iterator
    //!
    //! @param iter iterator pointing to the element to be erased
    //--------------------------------------------------------------------------
    void erase(maplocal_iterator_t iter);

    //--------------------------------------------------------------------------
    //! Number of entries in map
    //!
    //! @return number of entries in map
    //--------------------------------------------------------------------------
    uint64_t size() const;

    //--------------------------------------------------------------------------
    //! Count the elements with a specific key
    //!
    //! @param key key to search for
    //!
    //! @return 1 if container contains an element whose key is equivalent to
    //!         k, otherwise 0
    //--------------------------------------------------------------------------
    uint64_t count(const K& key) const;

    //--------------------------------------------------------------------------
    //! Get iterator to element
    //!
    //! @param key key to be searched for
    //!
    //! @return an iterator to the element, if an element with the specified
    //!         key is found, otherwise std::map::end
    //--------------------------------------------------------------------------
    maplocal_iterator_t find(const K& key);

    //--------------------------------------------------------------------------
    //! Get iterator to beginning of local map
    //--------------------------------------------------------------------------
    maplocal_iterator_t begin()
    {
       return mMap.begin();
    }

    //--------------------------------------------------------------------------
    //! Get iterator to end of local map
    //--------------------------------------------------------------------------
    maplocal_iterator_t end()
    {
       return mMap.end();
    }


    //--------------------------------------------------------------------------
    //! Get iterator to the next element
    //--------------------------------------------------------------------------
    maplocal_iterator_t next()
    {
      return mMap.next();
    }

    //--------------------------------------------------------------------------
    //! Get iterator to next element using the ++ operator
    //--------------------------------------------------------------------------
    maplocal_iterator_t operator ++()
    {
      return mMap.next();
    }

    //--------------------------------------------------------------------------
    //! Get string representation of the object
    //!
    //! @param value obj to be converted
    //!
    //! @return string representation of the object
    //--------------------------------------------------------------------------
    template <typename W>
    std::string ToString(W value) const;

    //--------------------------------------------------------------------------
    //! Convert string to type object
    //!
    //! @param value string object to be converted
    //!
    //! @return object obtained from converting the string
    //--------------------------------------------------------------------------
    template <typename W>
    W FromString(const std::string& sval) const;


  private:

    //! Declare class-wide constants
    static const std::string OBJ_EPOCH_KEY;
    static const std::string CHLOG_INSERT_OP;
    static const std::string CHLOG_ERASE_OP;
    //! Ratio between nuber of entries in the map and the nuber of entries in
    //! changelog when a compactification is done
    static const float COMPACTIFY_RATIO;

    std::map<K, V> mMap; ///< local representation of the map
    std::string mObjId;  ///< object id that holds the map information
    librados::IoCtx mIoCtx; ///< io context
    bool mPersistObj; /// < persist backend object (CEPH)
    uint64_t mEpoch; ///< current epoch of the local map
    uint64_t mChLogOff; ///< changelog offset of followed updates
    uint64_t mChLogNumLines; ///< number of entries in the changelog file

    //--------------------------------------------------------------------------
    //! Update the local contents of the map and the epoch if necessary
    //!
    //! @return true if update successful, otherwise false
    //--------------------------------------------------------------------------
    bool DoUpdate();

    //--------------------------------------------------------------------------
    //! Compactify the changelog
    //!
    //! @return true if compactification operation successful, otherwise false
    //--------------------------------------------------------------------------
    bool DoCompactify();

    //--------------------------------------------------------------------------
    //! Apply change log contents to the local map
    //!
    //! @param buffer containing the changes
    //!
    //! return true if successful, otherwise false
    //--------------------------------------------------------------------------
    bool ApplyChangeLog(const std::string& chlog);

    //--------------------------------------------------------------------------
    //! Helper function to convert string to non-string object.
    //!
    //! @param value string object to be converted
    //!
    //! @return object obtained from converting the string to type W
    //--------------------------------------------------------------------------
    template <typename W>
    bool HelperFromString(const std::string& sval, W& ret) const;

    //--------------------------------------------------------------------------
    //! Helper function to convert string to a string.
    //! Migth sound confugins but it's done like this so that the code compiles
    //! when the return value is either a string or a numeric type.
    //!
    //! @param value string object to be converted
    //!
    //! @return object obtained from converting the string to type W
    //--------------------------------------------------------------------------
    bool HelperFromString(const std::string& sval, std::string& ret) const;

    //--------------------------------------------------------------------------
    //! Helper function to get string representation of an object which is not
    //! a string.
    //!
    //! @param value obj to be converted
    //!
    //! @return string representation of the object
    //--------------------------------------------------------------------------
    template <typename W>
    bool HelperToString(W value, std::string& ret) const;

    //--------------------------------------------------------------------------
    //! Helper function to get string representation of a string. :)
    //! Might sound confusing but it's done like  this so that the code compiles
    //! when the return value is either a string or a numeric type.
    //!
    //! @param value obj to be converted
    //!
    //! @return string representation of the object
    //--------------------------------------------------------------------------
    bool HelperToString(std::string value, std::string& ret) const;

    //--------------------------------------------------------------------------
    //! Do a full map update with the values actually saved in the xattrs of
    //! the object and update at the same time the epoch and size of the obj.
    //!
    //! @return true if successful, otherwise false
    //--------------------------------------------------------------------------
    bool InitializeMap();

    //--------------------------------------------------------------------------
    //! Decide if changlog need compactification
    //!
    //! @return true if changlog needs compactification, otherwise false
    //--------------------------------------------------------------------------
    bool NeedsCompactification() const;

  };

  // Define the constants
  template <typename K, typename V>
  const std::string map< K, V>::OBJ_EPOCH_KEY {"obj_epoch_key"};

  template <typename K, typename V>
  const std::string map< K, V>::CHLOG_INSERT_OP {"+"};

  template <typename K, typename V>
  const std::string map< K, V>::CHLOG_ERASE_OP {"-"};

  template <typename K, typename V>
  const float map<K, V>::COMPACTIFY_RATIO {.2};


  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  map<K, V>::map(librados::Rados& rados_cluster,
                 const std::string& pool_name,
                 const std::string& name,
                 const std::string& cookie,
                 bool persist_obj) noexcept(false):
    mPersistObj(persist_obj),
    mEpoch(0),
    mChLogOff(0),
    mChLogNumLines(0)
  {
    // Check that we support the provided template parameters
    if (!std::is_same<std::string, K>::value ||
        !(std::is_same<unsigned long long, V>::value ||
          std::is_same<std::string, V>::value ||
          std::is_same<double, V>::value ||
          std::is_same<float, V>::value ||
          std::is_same<uint64_t, V>::value))
    {
      throw RadosContainerException("unsupported template parameter type");
    }

    int ret;
    std::ostringstream oss;
    oss << "/map/" << name << "/" << cookie;
    mObjId = oss.str();
    ret = rados_cluster.ioctx_create(pool_name.c_str(), mIoCtx);

    if (ret)
      throw RadosContainerException("unable to create ioctx for pool");

    // Check if object exists, if not create it
    uint64_t psize;
    time_t pmtime;

    if (mIoCtx.stat(mObjId, &psize, &pmtime))
    {
      if (mIoCtx.create(mObjId, true))
        throw RadosContainerException("unable to create obj.");

      // For new object set the epoch to 0
      std::map<std::string, librados::bufferlist> init_omap;
      init_omap[OBJ_EPOCH_KEY].append("0");
      mIoCtx.omap_set(mObjId, init_omap);
    }
    else
    {
      if (!InitializeMap())
        throw RadosContainerException("unable to get omap");
    }
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  map<K, V>::~map()
  {
    if (!mPersistObj && mIoCtx.remove(mObjId))
      throw RadosContainerException("unable to remove obj.");
  }

  //----------------------------------------------------------------------------
  // Count the number of entries
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  uint64_t map<K, V>::size() const
  {
    return mMap.size();
  }

  //----------------------------------------------------------------------------
  // Count the elements with a specific key
  //----------------------------------------------------------------------------
  template<typename K, typename V>
  uint64_t map<K, V>::count(const K& key) const
  {
    return mMap.count(key);
  }

  //----------------------------------------------------------------------------
  // Get iterator to element
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  typename std::map<K, V>::iterator
  map<K, V>::find(const K& key)
  {
    return mMap.find(key);
  }

  //----------------------------------------------------------------------------
  // Insert new value
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  std::pair<typename std::map<K, V>::iterator, bool>
  map<K, V>::insert(K key, V value)
  {
    // Check local epoch matches remote epoch
    int ret {1};
    int prval_cmp;

    // Prepare to changelog entry
    std::ostringstream oss;
    oss << CHLOG_INSERT_OP << " "
        << ToString(key) << " "
        << ToString(value) << std::endl;
    librados::bufferlist chlog_data;
    chlog_data.append(oss.str());

    while (ret)
    {
      librados::ObjectWriteOperation wr_op;
      std::map<std::string, std::pair<librados::bufferlist, int>> omap_assert;
      librados::bufferlist epoch_buff;
      epoch_buff.append(std::to_string(mEpoch));
      omap_assert[OBJ_EPOCH_KEY] = std::make_pair(epoch_buff, LIBRADOS_CMPXATTR_OP_EQ);
      wr_op.omap_cmp(omap_assert, &prval_cmp);

      // Update epoch
      std::map<std::string, librados::bufferlist> omap_upd;
      librados::bufferlist buff_epoch;
      buff_epoch.append(ToString<decltype(mEpoch)>(mEpoch + 1));
      omap_upd.insert(std::make_pair(OBJ_EPOCH_KEY, buff_epoch));
      wr_op.omap_set(omap_upd);

      // Write changelog entry
      wr_op.append(chlog_data);
      ret = mIoCtx.operate(mObjId, &wr_op);

      if (ret)
      {
        if (prval_cmp)
        {
          // Failed because of epoch missmatch - do an update and rerty
          fprintf(stderr, "Failed insert becasue of epoch missmatch - retry\n");

          // Update map and retry
          if (!DoUpdate())
            return std::make_pair(mMap.end(), false);
        }
        else
        {
          // Insert actually failed
          fprintf(stderr, "Fatal error during insert key=%s - abort\n", key.c_str());
          return std::make_pair(mMap.end(), false);
        }
      }
      else
      {
        // Update the local view of the changelog
        mEpoch++;
        mChLogNumLines++;
        mChLogOff += chlog_data.length();
      }
    }

    // Everything is up to date, do compactification if necessary
    if (NeedsCompactification() && !DoCompactify())
      fprintf(stderr, "Failed compactification - retry\n");

    // Insert into local map
    return mMap.insert(std::make_pair(key, value));
  }

  //----------------------------------------------------------------------------
  // Erase entry pointed by iterator
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  void map<K, V>::erase(typename std::map<K, V>::iterator iter)
  {
    erase(iter->first);
  }

  //----------------------------------------------------------------------------
  // Erase entry pointed by key
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  void
  map<K, V>::erase(K key)
  {
    int ret {1};
    int prval_cmp;

    // Prepare to changelog entry
    std::ostringstream oss;
    oss << CHLOG_ERASE_OP << " "
        << ToString(key)  << std::endl;
    librados::bufferlist chlog_data;
    chlog_data.append(oss.str());

    while (ret)
    {
      // Check local epoch matches remote epoch
      librados::ObjectWriteOperation wr_op;
      std::map<std::string, std::pair<librados::bufferlist, int>> omap_assert;
      librados::bufferlist epoch;
      epoch.append(std::to_string(mEpoch));
      omap_assert[OBJ_EPOCH_KEY] = std::make_pair(epoch, LIBRADOS_CMPXATTR_OP_EQ);
      wr_op.omap_cmp(omap_assert, &prval_cmp);

      // Update epoch
      std::map<std::string, librados::bufferlist> omap_upd;
      librados::bufferlist epoch_buff;
      epoch_buff.append(ToString<decltype(mEpoch)>(mEpoch +  1));
      omap_upd.insert(std::make_pair(OBJ_EPOCH_KEY, epoch_buff));
      wr_op.omap_set(omap_upd);

      // Write changelog entry
      wr_op.append(chlog_data);
      ret = mIoCtx.operate(mObjId, &wr_op);

      if (ret)
      {
        if (prval_cmp)
        {
          // Failed because of epoch missmatch - do an update and rerty
          fprintf(stderr, "Failed erase because of epoch missmatch - retry\n");

          // Update map and retry
          if (!DoUpdate())
            return;
        }
        else
        {
          // Any other error is fatal
          fprintf(stderr, "Fatal error during erase key=%s - abort\n", key.c_str());
          return;
        }
      }
      else
      {
        // Update the local view of the changelog
        mEpoch++;
        mChLogNumLines++;
        mChLogOff += chlog_data.length();
      }
    }

    // Local remove
    mMap.erase(key);

    // Everything is up to date, do compactification if necessary
    if (NeedsCompactification() && !DoCompactify())
      fprintf(stderr, "Failed compactification - retry\n");
  }

  //----------------------------------------------------------------------------
  // Get the full omap
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  bool map<K, V>::InitializeMap()
  {
    int ret {1};
    librados::bufferlist rd_buff;
    librados::bufferlist chlog_data;
    std::set<std::string> set_keys {OBJ_EPOCH_KEY};
    std::map<std::string, librados::bufferlist> omap_epoch;

    while (ret)
    {
      int prval_sz, prval_get;
      librados::ObjectReadOperation rd_stat;
      rd_stat.omap_get_vals_by_keys(set_keys, &omap_epoch, &prval_get);
      rd_stat.stat(&mChLogOff, nullptr, &prval_sz);
      ret = mIoCtx.operate(mObjId, &rd_stat, &rd_buff);

      // Get the current remote epoch
      if (ret)
      {
        // Highly unlikely
        fprintf(stderr, "The epoch search or stat operation failed!\n");
        return false;
      }

      // Convert remote epoch to numeric value if it exists
      if (omap_epoch.find(OBJ_EPOCH_KEY) == omap_epoch.end())
      {
        fprintf(stderr, "Fatal error, epoch tag not found in object map!\n");
        return false;
      }

      auto epoch_buff = omap_epoch[OBJ_EPOCH_KEY];
      mEpoch = FromString<uint64_t>(std::string(epoch_buff.c_str(),
                                                epoch_buff.length()));

      // Get the current offset of the object (i.e. changelog) and the entries
      // provided that the epoch didn't change
      int prval_cmp, prval_rd;
      rd_buff.clear();
      librados::ObjectReadOperation rd_op;
      std::map<std::string, std::pair<librados::bufferlist, int>> omap_assert;
      omap_assert[OBJ_EPOCH_KEY] = std::make_pair(epoch_buff, LIBRADOS_CMPXATTR_OP_EQ);
      rd_op.omap_cmp(omap_assert, &prval_cmp);
      rd_op.read(0, mChLogOff, &chlog_data, &prval_rd);

      // Execute operations atomically
      ret = mIoCtx.operate(mObjId, &rd_op, &rd_buff);

      if (ret)
      {
        if (prval_cmp)
        {
          fprintf(stderr, "Failed omap read because of epoch missmatch - retry\n");
          omap_epoch.clear();
          continue;
        }
        else
        {
          fprintf(stderr, "Fatal error during omap read operation\n");
          return false;
        }
      }
      else
      {
        // Replay the changelog to populate local map
        if (!ApplyChangeLog(std::string(chlog_data.c_str(), chlog_data.length())))
        {
          fprintf(stderr, "Fatal error while applying changelog!\n");
          return false;
        }

        fprintf(stderr, "Map epoch=%lu, log size=%lu, map_size=%lu\n",
                mEpoch, mChLogOff, mMap.size());
      }
    }

    return true;
  }

  //----------------------------------------------------------------------------
  // Update the local contents of the map and the epoch if necessary
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  bool map<K, V>::DoUpdate()
  {
    int ret {1};
    int prval_cmp;
    std::map<std::string, librados::bufferlist> omap_epoch;
    std::set<std::string> set_keys {OBJ_EPOCH_KEY};

    while (ret)
    {
      // Get the current remote epoch
      if (mIoCtx.omap_get_vals_by_keys(mObjId, set_keys, &omap_epoch))
      {
        // Highly unlikely
        fprintf(stderr, "The epoch value was not found in the map!\n");
        return false;
      }

      // Convert remote epoch to numeric value
      auto epoch_buff = omap_epoch[OBJ_EPOCH_KEY];
      uint64_t remote_epoch = FromString<uint64_t>(std::string(epoch_buff.c_str(),
                                                               epoch_buff.length()));

      if (mEpoch == remote_epoch)
      {
        return true;
      }
      else if (mEpoch < remote_epoch)
      {
        // Normal following of the changelog
        uint64_t psize;

        if (mIoCtx.stat(mObjId, &psize, nullptr))
        {
          // Highly unlikely
          fprintf(stderr, "Count not stat remote object=%s\n", mObjId.c_str());
          return false;
        }

        // Check that remote epoch is the same i.e. the size we just got
        // is correct
        librados::ObjectReadOperation rd_op;
        std::map<std::string, std::pair<librados::bufferlist, int>> omap_assert;
        librados::bufferlist epoch;
        librados::bufferlist rd_buff;
        epoch.append(std::to_string(remote_epoch));
        omap_assert[OBJ_EPOCH_KEY] = std::make_pair(epoch, LIBRADOS_CMPXATTR_OP_EQ);
        rd_op.omap_cmp(omap_assert, &prval_cmp);

        // Read the changelog from the cached size to the current remote size
        int prval_rd;
        librados::bufferlist chlog_data;
        rd_op.read(mChLogOff, psize - mChLogOff, &chlog_data, &prval_rd);
        ret = mIoCtx.operate(mObjId, &rd_op, &rd_buff);

        if (ret)
        {
          // Failed due to epoch missmatch - retry
          if (prval_cmp)
          {
            fprintf(stderr, "Failed update because of epoch missmatch - retry\n");
            continue;
          }
          else
          {
            fprintf(stderr, "Fatal error during update operation\n");
            return false;
          }
        }

        // Update cache changelog size to the current remote size
        mChLogOff = psize;

        // Update local map using the info from the read changelog
        if (!ApplyChangeLog(std::string(chlog_data.c_str(), chlog_data.length())))
        {
          fprintf(stderr, "Fatal error while applying changelog\n");
          return false;
        }

        // Update the local epoch to the remote epoch
        mEpoch = remote_epoch;
      }
      else
      {
        // Update after compactification done by someone else - meaning a full
        // reinitalisation of both the map and connected data structures
        if (!InitializeMap())
        {
          fprintf(stderr, "Fatal error while re-initialising the map after "
                  "a compactification operation\n");
          return false;
        }
        else
          break;
      }
    }

    return true;
  }

  //----------------------------------------------------------------------------
  // Compactify the changelog
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  bool map<K, V>::DoCompactify()
  {
    fprintf(stdout, "Do compactify, init chlog size=%lu\n", mChLogOff);
    int ret {1};
    bool ret_upd {false};
    std::ostringstream oss_dump;

    while ((ret_upd = DoUpdate()) && ret)
    {
      oss_dump.str("");
      oss_dump.clear();

      for (auto&& it: mMap)
      {
        oss_dump << CHLOG_INSERT_OP << " "
                 << ToString(it.first) << " "
                 << ToString(it.second) << std::endl;
      }

      librados::bufferlist chlog_data;
      chlog_data.append(oss_dump.str());

      // Provided that the epoch is correct truncate the changelog and
      // re-populate it with the entries from the local map and update the epoch
      int prval_cmp;
      librados::ObjectWriteOperation wr_op;
      std::map<std::string, std::pair<librados::bufferlist, int>> omap_assert;
      librados::bufferlist epoch_buff;
      epoch_buff.append(std::to_string(mEpoch));
      omap_assert[OBJ_EPOCH_KEY] = std::make_pair(epoch_buff, LIBRADOS_CMPXATTR_OP_EQ);
      wr_op.omap_cmp(omap_assert, &prval_cmp);

      // Compactify changelog
      wr_op.truncate(0);
      wr_op.write_full(chlog_data);

      // Update epoch to 0
      std::map<std::string, librados::bufferlist> omap_upd;
      epoch_buff.clear();
      epoch_buff.append(ToString((int) 0));
      omap_upd.insert(std::make_pair(OBJ_EPOCH_KEY, epoch_buff));
      wr_op.omap_set(omap_upd);
      ret = mIoCtx.operate(mObjId, &wr_op);

      if (ret)
      {
        if (prval_cmp)
        {
          fprintf(stderr, "Failed compactification because of epoch missmatch - "
                  "retry\n");
          continue;
        }
        else
        {
          fprintf(stderr, "Fatal error during compactification\n");
          return false;;
        }
      }
      else
      {
        mEpoch = 0;
        mChLogNumLines = mMap.size();
        mChLogOff = chlog_data.length();
        fprintf(stdout, "Do compactify, final chlog size=%lu\n", mChLogOff);
        return true;
      }
    }

    if (!ret_upd)
    {
      fprintf(stderr, "Failed update during compactification.\n");
      return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------
  // Apply changelog contents to the local map
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  bool map<K, V>::ApplyChangeLog(const std::string& chlog)
  {
    // If changelog data empty then return successful
    if (chlog.empty())
      return true;

    K key;
    V value;
    std::string entry, skey, svalue, action;
    std::istringstream iss(chlog);
    std::istringstream iss_entry;
    //fprintf(stderr, "chlog contents:\n%s\n", chlog.c_str());

    while (std::getline(iss, entry))
    {
      mChLogNumLines++;
      iss_entry.str(entry);
      iss_entry.clear();
      iss_entry >> action >> skey >> svalue;

      if (iss.fail())
        return false;

      key = FromString<K>(skey);

      if (action == CHLOG_INSERT_OP)
      {
        //fprintf(stderr, "Action=%s, key=%s, value=%s\n", action.c_str(),
        //        skey.c_str(), svalue.c_str());
        value = FromString<V>(svalue);
        (void) mMap.insert(std::make_pair(key, value));
      }
      else if (action == CHLOG_ERASE_OP)
      {
        //fprintf(stderr, "Action=%s, key=%s\n", action.c_str(), skey.c_str());
        (void) mMap.erase(key);
      }
      else
      {
        // Smth. really bad happened
        fprintf(stderr, "Found unkown action type in changlog\n");
      }
    }

    return true;
  }


  //----------------------------------------------------------------------------
  // Get string representation of the object
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  template <typename W>
  std::string map<K, V>::ToString(W value) const
  {
    std::string ret;

    if (!HelperToString(value, ret))
      throw RadosContainerException("unable to convert to string");

    return ret;
  }

  //----------------------------------------------------------------------------
  // Helper function to get string representation of an object which is not
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  template <typename W>
  bool map<K, V>::HelperToString(W value, std::string& ret) const
  {
    ret = std::to_string(value);
    return true;
  }

  //----------------------------------------------------------------------------
  // Helper function to get string representation of a string. :)
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  bool map<K, V>::HelperToString(std::string value, std::string& ret) const
  {
    ret = value;
    return true;
  }

  //----------------------------------------------------------------------------
  // Convert string to required representation
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  template <typename W>
  W map<K, V>::FromString(const std::string& sval) const
  {
    W ret;

    if (!HelperFromString(sval, ret))
      throw RadosContainerException("unable to convert from string");

    return ret;
  }

  //----------------------------------------------------------------------------
  // Helper function to convert string to number representation
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  template <typename W>
  bool map<K, V>::HelperFromString(const std::string& sval, W& ret) const
  {
    if (std::is_same<W, double>::value)
    {
      ret = std::stod(sval, nullptr);
      return true;
    }
    else if (std::is_same<W, float>::value)
    {
      ret = std::stof(sval, nullptr);
      return true;
    }
    else if (std::is_same<W, unsigned long long>::value ||
             std::is_same<W, uint64_t>::value)
    {
      ret = std::stoull(sval, nullptr);
      return true;
    }

    return false;
  }

  //----------------------------------------------------------------------------
  // Helper function to convert string to string representation
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  bool
  map<K, V>::HelperFromString(const std::string& sval, std::string& ret) const
  {
    ret = sval;
    return true;
  }

  //----------------------------------------------------------------------------
  // Decide if changlog need compactification
  //----------------------------------------------------------------------------
  template <typename K, typename V>
  bool map<K, V>::NeedsCompactification() const
  {
    return ((float) mMap.size() / mChLogNumLines <= COMPACTIFY_RATIO);
  }

}

#endif //__RADOSMAP_HH__
