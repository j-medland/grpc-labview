//---------------------------------------------------------------------
//---------------------------------------------------------------------
#include <grpc_server.h>
#include <cluster_copier.h>
#include <lv_interop.h>
#include <iostream>
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <assert.h>
#include <feature_toggles.h>

namespace grpc_labview
{
    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    void OccurServerEvent(LVUserEventRef event, gRPCid *data)
    {
        auto error = PostUserEvent(event, &data);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    void OccurServerEvent(LVUserEventRef event, gRPCid *data, const std::string &eventMethodName)
    {
        GeneralMethodEventData eventData;
        eventData.methodData = data;
        eventData.methodName = nullptr;

        SetLVString(&eventData.methodName, eventMethodName);

        auto error = PostUserEvent(event, &eventData);

        DSDisposeHandle(eventData.methodName);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    std::shared_ptr<MessageMetadata> CreateMessageMetadata(IMessageElementMetadataOwner *metadataOwner, LVMessageMetadata *lvMetadata)
    {
        std::shared_ptr<MessageMetadata> metadata(new MessageMetadata());

        auto name = GetLVString(lvMetadata->messageName);
        metadata->messageName = name;
        metadata->typeUrl = name;
        int clusterOffset = 0;
        if (lvMetadata->elements != nullptr)
        {
            // byteAlignment for LVMesageElementMetadata would be the size of its largest element which is a LStrHandle
            auto lvElement = (LVMesageElementMetadata *)(*lvMetadata->elements)->bytes(0, sizeof(LStrHandle));
            for (int x = 0; x < (*lvMetadata->elements)->cnt; ++x, ++lvElement)
            {
                auto element = std::make_shared<MessageElementMetadata>(metadataOwner);
                element->embeddedMessageName = GetLVString(lvElement->embeddedMessageName);
                element->protobufIndex = lvElement->protobufIndex;
                element->type = (LVMessageMetadataType)lvElement->valueType;
                element->isRepeated = lvElement->isRepeated;
                metadata->_elements.push_back(element);
                metadata->_mappedElements.emplace(element->protobufIndex, element);
            }
        }
        return metadata;
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    std::shared_ptr<MessageMetadata> CreateMessageMetadata2(IMessageElementMetadataOwner *metadataOwner, LVMessageMetadata2 *lvMetadata)
    {
        std::shared_ptr<MessageMetadata> metadata(new MessageMetadata());

        auto name = GetLVString(lvMetadata->messageName);
        metadata->messageName = name;
        metadata->typeUrl = GetLVString(lvMetadata->typeUrl);
        int clusterOffset = 0;
        if (lvMetadata->elements != nullptr)
        {
            // byteAlignment for LVMesageElementMetadata would be the size of its largest element which is a LStrHandle
            auto lvElement = (LVMesageElementMetadata *)(*lvMetadata->elements)->bytes(0, sizeof(LStrHandle));
            for (int x = 0; x < (*lvMetadata->elements)->cnt; ++x, ++lvElement)
            {
                auto element = std::make_shared<MessageElementMetadata>(metadataOwner);
                element->fieldName = GetLVString(lvElement->fieldName);
                element->embeddedMessageName = GetLVString(lvElement->embeddedMessageName);
                element->protobufIndex = lvElement->protobufIndex;
                element->type = (LVMessageMetadataType)lvElement->valueType;
                element->isRepeated = lvElement->isRepeated;
                metadata->_elements.push_back(element);
                metadata->_mappedElements.emplace(element->protobufIndex, element);
                element->isInOneof = lvElement->isInOneof;
                element->oneofContainerName = GetLVString(lvElement->oneofContainerName);
            }
        }
        return metadata;
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------

    std::vector<std::string> SplitString(std::string s, std::string delimiter)
    {
        size_t pos_start = 0, pos_end, delim_len = delimiter.length();
        std::string token;
        std::vector<std::string> res;

        while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos)
        {
            token = s.substr(pos_start, pos_end - pos_start);
            pos_start = pos_end + delim_len;
            res.push_back(token);
        }

        res.push_back(s.substr(pos_start));
        return res;
    }

    std::map<uint32_t, int32_t> CreateMapBetweenLVEnumAndProtoEnumvalues(std::string enumValues)
    {
        std::map<uint32_t, int32_t> lvEnumToProtoEnum;
        int seqLVEnumIndex = 0;
        for (std::string keyValuePair : SplitString(enumValues, ";"))
        {
            auto keyValue = SplitString(keyValuePair, "=");
            assert(keyValue.size() == 2);

            int protoEnumNumeric = std::stoi(keyValue[1]);
            lvEnumToProtoEnum.insert(std::pair<uint32_t, int32_t>(seqLVEnumIndex, protoEnumNumeric));
            seqLVEnumIndex += 1;
        }
        return lvEnumToProtoEnum;
    }

    void MapInsertOrAssign(std::map<int32_t, std::list<uint32_t>> *protoEnumToLVEnum, int protoEnumNumeric, std::list<uint32_t> lvEnumNumericValues)
    {
        auto existingElement = protoEnumToLVEnum->find(protoEnumNumeric);
        if (existingElement != protoEnumToLVEnum->end())
        {
            protoEnumToLVEnum->erase(protoEnumNumeric);
            protoEnumToLVEnum->insert(std::pair<int32_t, std::list<uint32_t>>(protoEnumNumeric, lvEnumNumericValues));
        }
        else
            protoEnumToLVEnum->insert(std::pair<int32_t, std::list<uint32_t>>(protoEnumNumeric, lvEnumNumericValues));
    }

    std::map<int32_t, std::list<uint32_t>> CreateMapBetweenProtoEnumAndLVEnumvalues(std::string enumValues)
    {
        std::map<int32_t, std::list<uint32_t>> protoEnumToLVEnum;
        int seqLVEnumIndex = 0;
        for (std::string keyValuePair : SplitString(enumValues, ";"))
        {
            auto keyValue = SplitString(keyValuePair, "=");
            int protoEnumNumeric = std::stoi(keyValue[1]);
            assert(keyValue.size() == 2);

            std::list<uint32_t> lvEnumNumericValues;
            auto existingElement = protoEnumToLVEnum.find(protoEnumNumeric);
            if (existingElement != protoEnumToLVEnum.end())
                lvEnumNumericValues = existingElement->second;

            lvEnumNumericValues.push_back(seqLVEnumIndex); // Add the new element

            MapInsertOrAssign(&protoEnumToLVEnum, protoEnumNumeric, lvEnumNumericValues);

            seqLVEnumIndex += 1;
        }
        return protoEnumToLVEnum;
    }

    std::shared_ptr<EnumMetadata> CreateEnumMetadata2(IMessageElementMetadataOwner *metadataOwner, LVEnumMetadata2 *lvMetadata)
    {
        std::shared_ptr<EnumMetadata> enumMetadata(new EnumMetadata());

        enumMetadata->messageName = GetLVString(lvMetadata->messageName);
        enumMetadata->typeUrl = GetLVString(lvMetadata->typeUrl);
        enumMetadata->elements = GetLVString(lvMetadata->elements);
        enumMetadata->allowAlias = lvMetadata->allowAlias;

        // Create the map between LV enum and proto enum values
        enumMetadata->LVEnumToProtoEnum = CreateMapBetweenLVEnumAndProtoEnumvalues(enumMetadata->elements);
        enumMetadata->ProtoEnumToLVEnum = CreateMapBetweenProtoEnumAndLVEnumvalues(enumMetadata->elements);

        return enumMetadata;
    }
}

int32_t ServerCleanupProc(grpc_labview::gRPCid *serverId);

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT void IsFeatureToggleSet(const char *filePath, uint8_t *isSet)
{
    *isSet = grpc_labview::FeatureConfig::getInstance().isFeatureEnabled(filePath) ? 1 : 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t LVCreateServer(grpc_labview::gRPCid **id)
{
    grpc_labview::InitCallbacks();
    auto server = new grpc_labview::LabVIEWgRPCServer();
    grpc_labview::gPointerManager.RegisterPointer(server);
    *id = server;
    grpc_labview::RegisterCleanupProc(ServerCleanupProc, server);
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t LVStartServer(char *address, char *serverCertificatePath, char *serverKeyPath, grpc_labview::gRPCid **id)
{
    auto server = (*id)->CastTo<grpc_labview::LabVIEWgRPCServer>();
    if (server == nullptr)
    {
        return -1;
    }
    return server->Run(address, serverCertificatePath, serverKeyPath);
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t LVGetServerListeningPort(grpc_labview::gRPCid **id, int *listeningPort)
{
    auto server = (*id)->CastTo<grpc_labview::LabVIEWgRPCServer>();
    if (server == nullptr)
    {
        return -1;
    }
    *listeningPort = server->ListeningPort();
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t LVStopServer(grpc_labview::gRPCid **id)
{
    auto server = (*id)->CastTo<grpc_labview::LabVIEWgRPCServer>();
    if (server == nullptr)
    {
        return -1;
    }
    server->StopServer();

    grpc_labview::DeregisterCleanupProc(ServerCleanupProc, *id);
    grpc_labview::gPointerManager.UnregisterPointer(server.get());
    return 0;
}

int32_t ServerCleanupProc(grpc_labview::gRPCid *serverId)
{
    return LVStopServer(&serverId);
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t RegisterMessageMetadata(grpc_labview::gRPCid **id, grpc_labview::LVMessageMetadata *lvMetadata)
{
    auto server = (*id)->CastTo<grpc_labview::MessageElementMetadataOwner>();
    if (server == nullptr)
    {
        return -1;
    }
    auto metadata = CreateMessageMetadata(server.get(), lvMetadata);
    server->RegisterMetadata(metadata);
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t RegisterMessageMetadata2(grpc_labview::gRPCid **id, grpc_labview::LVMessageMetadata2 *lvMetadata)
{
    auto server = (*id)->CastTo<grpc_labview::MessageElementMetadataOwner>();
    if (server == nullptr)
    {
        return -1;
    }
    auto metadata = CreateMessageMetadata2(server.get(), lvMetadata);
    server->RegisterMetadata(metadata);
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t RegisterEnumMetadata2(grpc_labview::gRPCid **id, grpc_labview::LVEnumMetadata2 *lvMetadata)
{
    auto server = (*id)->CastTo<grpc_labview::MessageElementMetadataOwner>();
    if (server == nullptr)
    {
        return -1;
    }
    auto metadata = CreateEnumMetadata2(server.get(), lvMetadata);
    server->RegisterMetadata(metadata);
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT uint32_t GetLVEnumValueFromProtoValue(grpc_labview::gRPCid **id, const char *enumName, int protoValue, uint32_t *lvEnumValue)
{
    auto server = (*id)->CastTo<grpc_labview::MessageElementMetadataOwner>();
    if (server == nullptr)
    {
        return -1;
    }
    auto metadata = (server.get())->FindEnumMetadata(std::string(enumName));
    *(uint32_t *)lvEnumValue = metadata.get()->GetLVEnumValueFromProtoValue(protoValue);

    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t GetProtoValueFromLVEnumValue(grpc_labview::gRPCid **id, const char *enumName, int lvEnumValue, int32_t *protoValue)
{
    auto server = (*id)->CastTo<grpc_labview::MessageElementMetadataOwner>();
    if (server == nullptr)
    {
        return -1;
    }
    auto metadata = (server.get())->FindEnumMetadata(std::string(enumName));
    *(int32_t *)protoValue = metadata.get()->GetProtoValueFromLVEnumValue(lvEnumValue);

    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t CompleteMetadataRegistration(grpc_labview::gRPCid **id)
{
    auto server = (*id)->CastTo<grpc_labview::MessageElementMetadataOwner>();
    if (server == nullptr)
    {
        return -1;
    }
    server->FinalizeMetadata();
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t RegisterServerEvent(grpc_labview::gRPCid **id, const char *name, grpc_labview::LVUserEventRef *item, const char *requestMessageName, const char *responseMessageName)
{
    auto server = (*id)->CastTo<grpc_labview::LabVIEWgRPCServer>();
    if (server == nullptr)
    {
        return -1;
    }

    server->RegisterEvent(name, *item, requestMessageName, responseMessageName);
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t RegisterGenericMethodServerEvent(grpc_labview::gRPCid **id, grpc_labview::LVUserEventRef *item)
{
    auto server = (*id)->CastTo<grpc_labview::LabVIEWgRPCServer>();
    if (server == nullptr)
    {
        return -1;
    }

    server->RegisterGenericMethodEvent(*item);
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t GetRequestData(grpc_labview::gRPCid **id, int8_t *lvRequest)
{
    auto data = (*id)->CastTo<grpc_labview::GenericMethodData>();
    if (data == nullptr)
    {
        return -1;
    }
    if (data->_call->IsCancelled())
    {
        return -(1000 + grpc::StatusCode::CANCELLED);
    }
    if (data->_call->IsActive() && data->_call->ReadNext())
    {
        try
        {
            grpc_labview::ClusterDataCopier::CopyToCluster(*data->_request, lvRequest);
        }
        catch (grpc_labview::InvalidEnumValueException &e)
        {
            // Before returning, set the call to complete, otherwise the server hangs waiting for the call.
            data->_call->ReadComplete();
            return e.code;
        }
        data->_call->ReadComplete();
        return 0;
    }
    return -2;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t SetResponseData(grpc_labview::gRPCid **id, int8_t *lvRequest)
{
    auto data = (*id)->CastTo<grpc_labview::GenericMethodData>();
    if (data == nullptr)
    {
        return -1;
    }
    try
    {
        grpc_labview::ClusterDataCopier::CopyFromCluster(*data->_response, lvRequest);
    }
    catch (grpc_labview::InvalidEnumValueException &e)
    {
        return e.code;
    }
    if (data->_call->IsCancelled())
    {
        return -(1000 + grpc::StatusCode::CANCELLED);
    }
    if (!data->_call->IsActive() || !data->_call->Write())
    {
        return -2;
    }
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t CloseServerEvent(grpc_labview::gRPCid **id)
{
    auto data = (*id)->CastTo<grpc_labview::GenericMethodData>();
    if (data == nullptr)
    {
        return -1;
    }
    data->NotifyComplete();
    data->_call->Finish();
    grpc_labview::gPointerManager.UnregisterPointer(*id);
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t SetCallStatus(grpc_labview::gRPCid **id, int grpcErrorCode, const char *errorMessage)
{
    auto data = (*id)->CastTo<grpc_labview::GenericMethodData>();
    if (data == nullptr)
    {
        return -1;
    }
    data->_call->SetCallStatusError((grpc::StatusCode)grpcErrorCode, errorMessage);
    return 0;
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
LIBRARY_EXPORT int32_t IsCancelled(grpc_labview::gRPCid **id)
{
    auto data = (*id)->CastTo<grpc_labview::GenericMethodData>();
    if (data == nullptr)
    {
        return -1;
    }
    return data->_call->IsCancelled();
}

//////////////////////////////////////////////////////////////////////////

#ifdef _PS_4
#pragma pack(push, 1)
#endif
struct LStrArray
{
    int32_t cnt;                     /* number of bytes that follow */
    grpc_labview::LStrHandle str[1]; /* cnt bytes */
};
#ifdef _PS_4
#pragma pack(pop)
#endif

LIBRARY_EXPORT void get_module_name(LStrArray **skip, grpc_labview::LVUserEventRef *ref, grpc_labview::LStr** test)
{
    
    typedef int (*PostLVUserEvent_T)(grpc_labview::LVUserEventRef ref, void *data);
    std::vector<std::string> skip_list;

    int32_t list_len = *skip && (*skip) ? (*skip)->cnt : 0;

    for(int32_t i=0; i<list_len; ++i){
        auto h = ((*skip)->str[i]);
        skip_list.emplace_back(&((*h)->str[0]), (*h)->cnt);
    }

    // Search for a Module which Exports the LabVIEW Functions we are after
    HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
    MODULEENTRY32 me32;

    //  Take a snapshot of all modules in the specified process.
    hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (hModuleSnap == INVALID_HANDLE_VALUE)
    {
        // snapshot failed
        return;
    }

    //  Set the size of the structure before using it.
    me32.dwSize = sizeof(MODULEENTRY32);

    //  Retrieve information about the first module
    if (!Module32First(hModuleSnap, &me32))
    {
        // first retrieval failed
        return;
    }

    //  Loop through, looking for a Module that exports the functions we are after
    do
    {
        std::string current(me32.szExePath);
        if(std::find(skip_list.begin(), skip_list.end(),current)==skip_list.end()){
            // unique
            auto x = (PostLVUserEvent_T)GetProcAddress(me32.hModule, "PostLVUserEvent");
            if(x){
                x(*ref, &list_len);
                std::memcpy(&((*test)->str[0]), current.data(), std::min(static_cast<size_t>((*test)->cnt), current.length()));
                (*test)->cnt = current.length();
            }
        }

    } while (Module32Next(hModuleSnap, &me32));

    CloseHandle(hModuleSnap);
    return;
}