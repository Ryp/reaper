////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>

#include "EntityDescriptor.h"
#include "core/Cast.h"

class AbstractWorldUpdater;

template <class ModuleType>
class ModuleAccessor
{
public:
    ModuleAccessor(std::map<EntityId, ModuleType>& modules)
        : _modules(modules)
    {}

    std::map<EntityId, ModuleType> const& map() const { return _modules; }

    ModuleType* operator[](EntityId id)
    {
        Assert(_modules.count(id) > 0, "entity id not found");
        return &_modules.at(id);
    }

    ModuleType const* operator[](EntityId id) const
    {
        Assert(_modules.count(id) > 0, "entity id not found");
        return &_modules.at(id);
    }

private:
    std::map<EntityId, ModuleType>& _modules;
};

class AbstractModuleUpdater
{
public:
    virtual ~AbstractModuleUpdater() {}
    virtual void createModule(EntityId id, const ModuleDescriptor* descriptor) = 0;
};

template <class ModuleType, class DescriptorType>
class ModuleUpdater : public AbstractModuleUpdater
{
public:
    ModuleUpdater(AbstractWorldUpdater* worldUpdater)
        : _worldUpdater(worldUpdater)
    {}
    virtual ~ModuleUpdater() {}

public:
    ModuleAccessor<ModuleType> getModuleAccessor() { return ModuleAccessor<ModuleType>(_modules); }

    void createModule(EntityId id, const ModuleDescriptor* descriptor) override
    {
        createModule(id, checked_cast<const DescriptorType*>(descriptor));
    }

    virtual void removeModule(EntityId id)
    {
        if (_modules.count(id) > 0)
            _modules.erase(id);
    }

protected:
    virtual void createModule(EntityId id, const DescriptorType* descriptor) = 0;
    void         addModule(EntityId id, ModuleType& module)
    {
        Assert(_modules.count(id) == 0, "entity id already taken");
        _modules.emplace(std::pair<EntityId, ModuleType>(id, module));
    }

protected:
    AbstractWorldUpdater*          _worldUpdater;
    std::map<EntityId, ModuleType> _modules;
};
