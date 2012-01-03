#!/usr/bin/env python

##############################################################################
##
## This file is part of Sardana
##
## http://www.tango-controls.org/static/sardana/latest/doc/html/index.html
##
## Copyright 2011 CELLS / ALBA Synchrotron, Bellaterra, Spain
## 
## Sardana is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
## 
## Sardana is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
## 
## You should have received a copy of the GNU Lesser General Public License
## along with Sardana.  If not, see <http://www.gnu.org/licenses/>.
##
##############################################################################

"""This module contains the class definition for the MacroServer environment
manager"""

__all__ = ["EnvironmentManager"]

__docformat__ = 'restructuredtext'

import os
import re
import shelve
import copy
import operator
import types

from taurus.core import ManagerState
from taurus.core.util import Singleton, Logger, CaselessDict, CodecFactory

from exception import UnknownEnv


class EnvironmentManager(Singleton, Logger):
    """The MacroServer environment manager class. It is designed to be a 
    singleton for the entire application.
    """
    
    def __init__(self, *args):
        """ Initialization. Nothing to be done here for now."""
        pass

    def init(self, *args, **kwargs):
        """Singleton instance initialization."""
        name = self.__class__.__name__
        self._state = ManagerState.UNINITIALIZED
        self.call__init__(Logger, name)
        
        # a string containing the absolute filename containing the environment
        self._env_name = None
        
        # the full enviroment (a shelf for now - can be accessed as a dict)
        self._env = None
        
        # cache environment for keys that start with door name
        # dict<string, dict<string, value> > where:
        #  - key: door name
        #  - value: dict where:
        #    - key: environment name
        #    - value: environment value
        self._door_env = None

        # cache environment for keys that start with macro name
        # dict<string, dict<string, value> > where:
        #  - key: macro name
        #  - value: dict where:
        #    - key: environment name
        #    - value: environment value
        self._macro_env = None
        
        # cache environment for global keys
        # dict<string, value> where:
        #  - key: environment name
        #  - value: environment value
        self._global_env = None

        self.reInit(*args)

    def reInit(self):
        """(Re)initializes the manager"""
        if self._state == ManagerState.INITED:
            return
        
        self._initEnv()
        
        self._state = ManagerState.INITED
    
    def cleanUp(self):
        if self._state == ManagerState.CLEANED:
            return

        self._clearEnv()

        self._state = ManagerState.CLEANED

    def _initEnv(self):
        self._macro_env, self._global_env = {}, {}
        self._door_env = CaselessDict()

    def _clearEnv(self):
        self._env = self._macro_env = self._global_env = self._door_env = None
            
    def setEnvironment(self, f_name):
        """Sets up a new environment from a file"""
        self._initEnv()
        f_name = os.path.abspath(f_name)
        self._env_name = f_name
        dir_name = os.path.dirname(f_name)
        if not os.path.isdir(dir_name):
            try:
                self.info("Creating environment directory: %s" % dir_name)
                os.makedirs(dir_name)
            except OSError, ose:
                self.error("Creating environment: %s" % ose.strerror)
                raise ose
        self.info("Environment is being stored in %s", f_name)
        
        self._env = shelve.open(f_name, flag='c', protocol=0, writeback=False)
        # fill the three environment caches
        self._fillEnvironmentCaches(self._env)

    def _fillEnvironmentCaches(self, env):
        # fill the three environment caches
        for k, v in env.iteritems():
            k_parts = k.split('.', 1)
            key = k_parts[0]
            env_dict = self._global_env
            
            # door or macro property
            if len(k_parts) == 2:
                if k_parts[0].count('/') == 2:
                    d = self._door_env
                else:
                    d = self._macro_env
                super_key, key = k_parts
                if not d.has_key(super_key):
                    d[super_key] = {}
                env_dict = d[super_key]

            env_dict[key] = v

    def hasEnv(self, key, macro_name=None, door_name=None):
        #<door>.<macro>.<property name> (highest priority)
        if macro_name and door_name:
            has = self._hasDoorMacroPropertyEnv((door_name, macro_name, key))
            if has: return True
            
        # <macro>.<property name>
        if macro_name:
            has = self._hasMacroPropertyEnv((macro_name, key))
            if has: return True
        
        # <door>.<property name>
        if door_name:
            has = self._hasDoorPropertyEnv((door_name, key))
            if has: return True
        
        # <property name> (less priority)
        return self._hasEnv(key)

    def _getDoorMacroPropertyEnv(self, prop):
        """Returns the property value for a property which must have the format
        <door name>.<macro name>.<property name>"""
        if isinstance(prop, str):
            door_name, macro_name_key = prop.split('.', 1)
        else:
            door_name, macro_name_key = prop[0], '.'.join(prop[1:])
        door_props = self._door_env.get(door_name)
        if door_props is None:
            return None
        return door_props.get(macro_name_key)

    def _hasDoorMacroPropertyEnv(self, prop):
        """Determines if the environment contains a property with the format
        <door name>.<macro name>.<property name>"""
        return not self._getDoorMacroPropertyEnv() is None

    def _getMacroPropertyEnv(self, prop):
        """Returns the property value for a property which must have the format
        <macro name>.<property name>"""
        if isinstance(prop, str):
            macro_name, key = prop.split('.')
        else:
            macro_name, key = prop
        macro_props = self._macro_env.get(macro_name)
        if macro_props is None:
            return None
        return macro_props.get(key)

    def _hasMacroPropertyEnv(self, prop):
        """Determines if the environment contains a property with the format
        <macro name>.<property name>"""
        return not self._getMacroPropertyEnv() is None
    
    def _getDoorPropertyEnv(self, prop):
        """Returns the property value for a property which must have the format
        <door name>.<property name>"""
        if isinstance(prop, str):
            door_name, key = prop.split('.')
        else:
            door_name, key = prop
        door_props = self._door_env.get(door_name)
        if door_props is None:
            return None
        return door_props.get(key)

    def _hasDoorPropertyEnv(self, prop):
        """Determines if the environment contains a property with the format
        <door name>.<property name>"""
        return not self._getDoorPropertyEnv() is None

    def _getEnv(self, prop):
        """Returns the property value for a property which must have the format
        <property name>"""
        return self._global_env.get(prop)

    def _hasEnv(self, prop):
        """Determines if the environment contains a property with the format
        <property name>"""
        return not self._getEnv(prop) is None

    def getEnv(self, key=None, door_name=None, macro_name=None):
        """Gets the environment matching the given parameters:
        - If key is None it returns the complete environment for the given 
          macro and/or door. If both are None the the complete environment is
          returned
          @param[in]"""
        if key is None:
            return self._getAllEnv(door_name=door_name, macro_name=macro_name)
        
        #<door>.<macro>.<property name> (highest priority)
        if macro_name and door_name:
            v = self._getDoorMacroPropertyEnv((door_name, macro_name, key))
            if not v is None: return v
            
        # <macro>.<property name>
        if macro_name:
            v = self._getMacroPropertyEnv((macro_name, key))
            if not v is None: return v
        
        # <door>.<property name>
        if door_name:
            v = self._getDoorPropertyEnv((door_name, key))
            if not v is None: return v
        
        # <property name> (less priority)
        v = self._getEnv(key)
        if v is None:
            raise UnknownEnv("Unknown environment %s" % key)
        return v

    def _getAllEnv(self, door_name=None, macro_name=None):
        """Gets the complete environment for the given macro and/or door. If 
        both are None the the complete environment is returned"""
        if macro_name is None and door_name is None:
            return dict(self._env)
        elif not door_name is None and macro_name is None:
            return self.getDoorEnv(door_name)
        elif door_name and macro_name:
            return self.getAllDoorMacroEnv(door_name, macro_name)
        elif not macro_name is None and door_name is None:
            return self._macro_env.get(macro_name, {})

    def getAllDoorEnv(self, door_name):
        """Gets the complete environment for the given door."""
        door_name = door_name.lower()
        
        # first go through the global environment
        ret = self._global_env.copy()
        
        # Then go through the door specific environment
        ret.update( self._door_env.get(door_name, {}) )
        return ret

    def getAllDoorMacroEnv(self, door_name, macro_name):
        """Gets the complete environment for the given macro in a specific door.

        @param[in] door_name a string with the door name (case insensitive)
        @param[in] macro_name a string with the macro name
        
        @return a dictionary with the resulting environment"""
        door_name = door_name.lower()
        
        # first go through the global environment
        ret = self._global_env.copy()
    
        # get the specific door environment
        d_env = self._door_env.get(door_name, {})

        # get the specific macro environment
        m_env = self._macro_env.get(macro_name, {})
        
        # put the doors global environment 
        for k, v in d_env.iteritems():
            if k.count('.') == 0:
                ret[k] = V
        
        # put the macro environment
        ret.update(m_env)
        
        # put the door and macro specific environment
        for k, v in d_env.iteritems():
            if k.count('.') > 0:
                m_name, key = k.split('.',1)
                if m_name is macro_name:
                    ret[key] = v
            
        return ret

    def getDoorMacroEnv(self, door_name, macro_name, keys=None):
        """Gets the environment for the given macro in a specific door for the
        given key(s)
        
        @param[in] door_name a string with the door name (case insensitive)
        @param[in] macro_name a string with the macro name
        @param[in] key the keys to be retrieved. If None (default) the complete
                       environment is returned (same as getAllDoorMacroEnv)
                       key can be a string or a sequence<string>.
                       keys must NOT contain '.' characters
        
        @return a dictionary with the resulting environment"""
        if keys is None:
            return getAllDoorMacroEnv(door_name, macro_name)
        
        if type(keys) in types.StringTypes:
            keys = (keys,)
        
        door_name = door_name.lower()

        g_env = self._global_env
        # get the specific door environment
        d_env = self._door_env.get(door_name, {})
        # get the specific macro environment
        m_env = self._macro_env.get(macro_name, {})
                
        # first go through the global environment
        ret = {} 
        for k in keys:
            comp_key = '%s.%s' % (macro_name, k)
            if d_env.has_key(comp_key):
                ret[k] = d_env[comp_key]
            elif m_env.has_key(k):
                ret[k] = m_env[k]
            elif d_env.has_key(k):
                ret[k] = d_env[k]
            elif g_env.has_key(k):
                ret[k] = g_env[k]
        
        return ret

    def _pairwise(self, iterable):
        itnext = iter(iterable).next
        while True:
            yield itnext(), itnext()
            
    def _dictFromSequence(self, seq):
        return dict(self._pairwise(seq))

    def _encode(self, d):
        ret = {}
        for k, v in d.iteritems():
            if type(v) in types.StringTypes:
                try:
                    v = eval(v)
                except:
                    v_lower = v.lower()
                    try:
                        v = eval(v_lower.capitalize())
                    except:
                        pass
            ret[k] = v
        return ret

    def _getCacheForKey(self, key):
        """Returns the cache dictionary object for the given key
        @param[in] key a string representing the key 
        @return a tuple pair. The first element is the dictionary and the second
                is the modified key that is applicable to the dictionary"""
        d = None
        key_parts = key.split('.')
        # global property
        if len(key_parts) == 1:
            d = self._global_env
        # macro property
        elif len(key_parts) == 2 and key_parts[0].count('/') != 2:
            macro_name, key = key_parts
            d = self._macro_env.get(macro_name)
            if d is None:
                self._macro_env[macro_name] = d = {}
        # door property
        else:
            door_name, key = key.split('.', 1)
            d = self._door_env.get(door_name)
            if d is None:
                self._door_env[door_name] = d = {}
        return d, key
    
    def _setOneEnv(self, key, value):
        self._env[key] = value
        self._env.sync()
        d, key = self._getCacheForKey(key)
        d[key] = value
        
    def _unsetOneEnv(self, key):
        if not self._env.has_key(key):
            raise UnknownEnv("Unknown environment %s" % key)
        del self._env[key]
        self._env.sync()
        d, key = self._getCacheForKey(key)
        if d.has_key(key):
            del d[key]
        
    def _unsetEnv(self, env_names):
        for key in env_names:
            self._unsetOneEnv(key)

    def setEnvObj(self, obj):
        """Sets the environment for the given object. If object is a sequence 
        then each pair of elements k, v is added as env[k] = v.
        If object is a map then the environmnent is updated.
        Other object types are not supported
        The elements which are strings are 'python evaluated'
        
        @throws TypeError is obj is not a sequence or a map
        
        @param[in] obj object to be added to the environment
        
        @return a dict representing the added environment"""
        
        if operator.isSequenceType(obj) and not type(obj) in types.StringTypes:
            obj = self._dictFromSequence(obj)
        elif not operator.isMappingType(obj):
            raise TypeError("obj parameter must be a sequence or a map")
        
        obj = self._encode(obj)
        for k, v in obj.iteritems():
            self._setOneEnv(k, v)
        obj["__type__"] = "new"
        codec = CodecFactory().getCodec('json')
        data = codec.encode(('',obj))

        # @TODO find another way to find existing doors without connecting to tango
        import PyTango
        for door in PyTango.Util.instance().get_device_list_by_class("Door"):
            door.sendEnvironment(*data)
        return obj
    
    def setEnv(self, key, value):
        """Sets the environment key to the new value and stores it persistently.
        
        @param[in] key the key for the environment
        @param[in] value the value for the environment
        
        @return a tuple with the key and value objects stored"""
        ret = self.setEnvObj((key,value))
        return key, ret[key]

    def unsetEnv(self, key):
        """Unsets the environment for the given key.
        @param[in] key the key for the environment to be unset"""
        if type(key) in types.StringTypes:
            key = (key,)
        self._unsetEnv(key)