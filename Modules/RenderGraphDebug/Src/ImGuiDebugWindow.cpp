/**
 * @file
 * @author Max Godefroy
 * @date 27/09/2025.
 */

#include "KryneEngine/Modules/RenderGraph/ImGuiDebugWindow.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <EASTL/vector_map.h>
#include <EASTL/sort.h>
#include <imgui.h>
#include <KryneEngine/Core/Profiling/TracyHeader.hpp>
#include <KryneEngine/Core/Memory/DynamicArray.hpp>

#include "KryneEngine/Modules/RenderGraph/Builder.hpp"
#include "KryneEngine/Modules/RenderGraph/Registry.hpp"
#include "KryneEngine/Modules/RenderGraph/Resource.hpp"

namespace KryneEngine::Modules::RenderGraph
{
    static constexpr u32 kFakeVertexFlag = 1u << 31;
    inline u32 GetFlatArrayIndex(u32 _nodeIndex, u32 _passCount)
    {
        return _nodeIndex & kFakeVertexFlag
            ? (_nodeIndex & ~kFakeVertexFlag) + _passCount
            : _nodeIndex;
    }

    void ImGuiDebugWindow::DebugBuilder(
        const Builder& _builder,
        const Registry& _registry,
        AllocatorInstance _tempAllocator,
        bool* _windowOpen)
    {
        KE_ASSERT(_builder.m_isBuilt);

        if (!ImGui::Begin("Render Graph Builder", _windowOpen))
        {
            ImGui::End();
            return;
        }

        if (ImGui::BeginTabBar("BuilderDebugTabBar"))
        {
            if (ImGui::BeginTabItem("Passes"))
            {
                DisplayBuilderPasses(_builder, _registry, _tempAllocator);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Resources"))
            {
                DisplayBuilderResources(_builder, _registry, _tempAllocator);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void ImGuiDebugWindow::DisplayBuilderPasses(
        const Builder& _builder,
        const Registry& _registry,
        AllocatorInstance _tempAllocator)
    {
        KE_ZoneScopedFunction("ImGuiDebugWindow::DisplayBuilderPasses");

        static ImVec2 relativeOffset {};
        if (ImGui::IsWindowAppearing())
        {
            relativeOffset = { ImGui::GetWindowSize().x / 2.f, 30.f };
        }

        static bool cullUnusedPasses = false;
        ImGui::Checkbox("Cull unused passes", &cullUnusedPasses);

        static s32 reorderIterations = 23;
        if (ImGui::TreeNode("Advanced"))
        {
            ImGui::SliderInt("Reorder iterations", &reorderIterations, 0, 23);
            ImGui::TreePop();
        }

        static u64 selectedPass = 0;

        constexpr u32 invalid = ~0u;
        constexpr float horizontalSpacing = 20.f;
        constexpr float verticalSpacing = 20.f;
        constexpr float minNodeWidth = 50.f;
        constexpr u32 fakeVertexFlag = 1u << 31;
        const ImVec2 padding { 8.f, 8.f };
        const u32 passCount = _builder.m_declaredPasses.size();
        const float nodeHeight = ImGui::GetTextLineHeight() * 2 + padding.y * 2 + 2.f;
        DynamicArray<u32> layersIndices(_tempAllocator, passCount, invalid);

        struct Node
        {
            u32 m_index;
            float m_width;
        };

        struct Link
        {
            u32 m_parent;
            u32 m_child;
            bool m_selected;
        };

        struct Layer
        {
            float m_totalWidth = 0.f;
            eastl::vector<Node> m_nodes{};
            eastl::vector<Link> m_downwardLinks {};
        };
        eastl::vector<Layer> layers;
        {
            KE_ZoneScoped("Generate layers");

            for (u32 i = 0; i < passCount; i++)
            {
                if (cullUnusedPasses && !_builder.m_passAlive[i])
                {
                    continue;
                }

                if (_builder.m_dag[i].m_parents.empty())
                {
                    layersIndices[i] = 0;
                }
                else
                {
                    u32 maxParentLayer = 0;
                    for (const u32 parent : _builder.m_dag[i].m_parents)
                    {
                        KE_ASSERT(layersIndices[parent] != invalid);

                        if (layersIndices[parent] > maxParentLayer)
                        {
                            maxParentLayer = layersIndices[parent];
                        }
                    }
                    layersIndices[i] = maxParentLayer + 1;
                }

                if (layersIndices[i] >= layers.size())
                {
                    layers.resize(
                        layersIndices[i] + 1, Layer{.m_nodes{_tempAllocator}, .m_downwardLinks{_tempAllocator}});
                }

                const u32 layerIndex = layersIndices[i];
                Layer& layer = layers[layerIndex];

                const float nodeWidth = eastl::max(
                    ImGui::CalcTextSize(_builder.m_declaredPasses[i].m_name.m_string.c_str()).x + 2 * padding.x,
                    minNodeWidth);
                layer.m_nodes.emplace_back(i, nodeWidth);
                layer.m_totalWidth += nodeWidth;
            }
        }

        u32 fakeVertexCount = 0;
        {
            KE_ZoneScoped("Generate links and fake vertices");

            for (u32 i = 0; i < layers.size(); i++)
            {
                Layer& layer = layers[i];
                for (const Node& node : layer.m_nodes)
                {
                    if (node.m_index & fakeVertexFlag)
                    {
                        continue;
                    }

                    for (u32 child : _builder.m_dag[node.m_index].m_children)
                    {
                        const bool selected = selectedPass == _builder.m_declaredPasses[child].m_name.m_hash
                                              || selectedPass == _builder.m_declaredPasses[node.m_index].m_name.m_hash;
                        ;
                        u32 currentNode = node.m_index;
                        u32 j = i;
                        for (; j + 1 < layersIndices[child]; j++)
                        {
                            const u32 newNode = (fakeVertexCount++) | fakeVertexFlag;
                            layers[j].m_downwardLinks.push_back(
                                Link{
                                    .m_parent = currentNode,
                                    .m_child = newNode,
                                    .m_selected = selected,
                                });
                            layers[j + 1].m_nodes.emplace_back(newNode, 0.f);
                            currentNode = newNode;
                        }
                        layers[j].m_downwardLinks.emplace_back(currentNode, child, selected);
                    }
                }
            }
        }

        struct HorizontalPosition
        {
            float m_center;
            float m_halfWidth;
        };
        eastl::vector<HorizontalPosition> horizontalPositions(passCount + fakeVertexCount, _tempAllocator);
        {
            KE_ZoneScoped("Set final node horizontal positioning");
            for (Layer& layer : layers)
            {
                layer.m_totalWidth += horizontalSpacing * static_cast<float>(layer.m_nodes.size() - 1);
                float currentOffset = -layer.m_totalWidth / 2.f;
                for (Node& node : layer.m_nodes)
                {
                    const u32 hoIndex = GetFlatArrayIndex(node.m_index, passCount);
                    horizontalPositions[hoIndex] = {
                        currentOffset + node.m_width * 0.5f,
                        node.m_width * 0.5f};
                    currentOffset += node.m_width + horizontalSpacing;
                }
            }
        }

        {
            KE_ZoneScoped("Vertex reordering");

            // Since optimal reordering is a NP-complete problem, we will be using heuristics instead.
            // The most common heuristic is the median method, used, for instance, by the dot engine in GraphViz.
            // In short, we run multiple iterations downwards (then upwards) where we reposition the nodes based on
            // their children (then parents) median position.
            //
            // If there is no perfect solution without at least one vertex crossing, there won't be a clean convergence.
            // The heuristic iteration will oscillate between "good enough" orderings.
            //
            // There is a variant using the average, but it apparently provides worse results with slower convergence.

            // Fake vertices have 1 parent and one child, so we can initialize the arrays with 1 to quickly handle them.
            DynamicArray<u32> parentCounts(_tempAllocator, passCount + fakeVertexCount, 1);
            DynamicArray<u32> childCounts(_tempAllocator, passCount + fakeVertexCount, 1);
            for (u32 i = 0; i < passCount; i++)
            {
                parentCounts[i] = _builder.m_dag[i].m_parents.size();
                childCounts[i] = _builder.m_dag[i].m_children.size();
            }

            // Use flat arrays for smaller memory footprint and cache performance.
            u32 maxFlatMedianArraySize = 0;
            u32 maxNodeCount = 0;
            for (auto& layer : layers)
            {
                u32 parentCount = 0;
                u32 childCount = 0;
                for (const Node& node : layer.m_nodes)
                {
                    parentCount += parentCounts[GetFlatArrayIndex(node.m_index, passCount)];
                    childCount += childCounts[GetFlatArrayIndex(node.m_index, passCount)];
                }

                maxFlatMedianArraySize = eastl::max(maxFlatMedianArraySize, eastl::max(parentCount, childCount));
                maxNodeCount = eastl::max<u32>(maxNodeCount, layer.m_nodes.size());
            }

            DynamicArray<float> flatMedianArrays(_tempAllocator, maxFlatMedianArraySize);
            DynamicArray<u32> vertexLayerIndex(_tempAllocator, passCount + fakeVertexCount);
            eastl::vector<eastl::span<float>> medianArrayPerNode(_tempAllocator);
            medianArrayPerNode.resize(maxNodeCount);

            for (u32 h = 0; h < reorderIterations; h++)
            {
                const auto repositionNodes = [&](Layer& _layer)
                {
                    const u32 nodeCount = _layer.m_nodes.size();
                    for (u32 j = 0; j < nodeCount; j++)
                    {
                        const eastl::span<float>& medianSpan = medianArrayPerNode[j];

                        if (medianSpan.empty())
                            continue;

                        float newPosition;
                        if (medianSpan.size() & 1)
                        {
                            newPosition = medianSpan[medianSpan.size() / 2];
                        }
                        else
                        {
                            newPosition = (medianSpan[medianSpan.size() / 2 - 1] + medianSpan[medianSpan.size() / 2]) * 0.5f;
                        }
                        horizontalPositions[GetFlatArrayIndex(_layer.m_nodes[j].m_index, passCount)].m_center = newPosition;
                    }

                    if (nodeCount <= 1)
                    {
                        return;
                    }

                    eastl::sort(
                        _layer.m_nodes.begin(),
                        _layer.m_nodes.end(),
                        [&horizontalPositions, passCount](const Node& _a, const Node& _b)
                        {
                            return horizontalPositions[GetFlatArrayIndex(_a.m_index, passCount)].m_center
                                   < horizontalPositions[GetFlatArrayIndex(_b.m_index, passCount)].m_center;
                        });
                    u32 leftShiftRBegin;
                    u32 rightShiftBegin;
                    float leftShift = 0.f;
                    float rightShift = 0.f;
                    if (nodeCount & 1)
                    {
                        leftShiftRBegin = nodeCount / 2 - 1;
                        rightShiftBegin = nodeCount / 2 + 1;
                        const HorizontalPosition& centerPos = horizontalPositions[GetFlatArrayIndex(
                            _layer.m_nodes[nodeCount / 2].m_index, passCount)];
                        leftShift = centerPos.m_center - centerPos.m_halfWidth - horizontalSpacing;
                        rightShift = centerPos.m_center + centerPos.m_halfWidth + horizontalSpacing;
                    }
                    else
                    {
                        leftShiftRBegin = nodeCount / 2 - 1;
                        rightShiftBegin = nodeCount / 2;
                        const HorizontalPosition& centerPosA = horizontalPositions[GetFlatArrayIndex(
                            _layer.m_nodes[leftShiftRBegin].m_index, passCount)];
                        const HorizontalPosition& centerPosB = horizontalPositions[GetFlatArrayIndex(
                            _layer.m_nodes[rightShiftBegin].m_index, passCount)];
                        const float center = (centerPosA.m_center + centerPosB.m_center) * 0.5f;
                        leftShift = center - horizontalSpacing * 0.5f;
                        rightShift = center + horizontalSpacing * 0.5f;
                    }

                    for (s32 idx = static_cast<s32>(leftShiftRBegin); idx >= 0; idx--)
                    {
                        HorizontalPosition& pos = horizontalPositions[GetFlatArrayIndex(_layer.m_nodes[idx].m_index, passCount)];
                        pos.m_center = eastl::min(pos.m_center, leftShift - pos.m_halfWidth);
                        leftShift = pos.m_center - pos.m_halfWidth - horizontalSpacing;
                    }
                    for (u32 idx = rightShiftBegin; idx < nodeCount; idx++)
                    {
                        HorizontalPosition& pos = horizontalPositions[GetFlatArrayIndex(_layer.m_nodes[idx].m_index, passCount)];
                        pos.m_center = eastl::max(pos.m_center, rightShift + pos.m_halfWidth);
                        rightShift = pos.m_center + pos.m_halfWidth + horizontalSpacing;
                    }
                };

                // Apply downwards first
                for (u32 i = 0; i < layers.size() - 1; i++)
                {
                    medianArrayPerNode.clear();
                    u32 totalFlatArraySize = 0;
                    for (u32 j = 0; j < layers[i+1].m_nodes.size(); j++)
                    {
                        const u32 nodeIndex = layers[i+1].m_nodes[j].m_index;
                        vertexLayerIndex[GetFlatArrayIndex(nodeIndex, passCount)] = j;
                        const u32 parentCount = parentCounts[GetFlatArrayIndex(nodeIndex, passCount)];;
                        medianArrayPerNode.emplace_back(flatMedianArrays.Data() + totalFlatArraySize, 0);
                        totalFlatArraySize += parentCount;
                    }

                    for (const Link& link: layers[i].m_downwardLinks)
                    {
                        eastl::span<float>& medianSpan = medianArrayPerNode[vertexLayerIndex[GetFlatArrayIndex(link.m_child, passCount)]];
                        *medianSpan.end() = horizontalPositions[GetFlatArrayIndex(link.m_parent, passCount)].m_center;
                        medianSpan = { medianSpan.begin(), medianSpan.end() + 1 };
                    }

                    repositionNodes(layers[i + 1]);
                }

                // Apply upwards next
                for (u32 i = layers.size() - 1; i > 0; i--)
                {
                    medianArrayPerNode.clear();
                    u32 totalFlatArraySize = 0;
                    for (u32 j = 0; j < layers[i-1].m_nodes.size(); j++)
                    {
                        const u32 nodeIndex = layers[i-1].m_nodes[j].m_index;
                        vertexLayerIndex[GetFlatArrayIndex(nodeIndex, passCount)] = j;
                        const u32 parentCount = parentCounts[GetFlatArrayIndex(nodeIndex, passCount)];;
                        medianArrayPerNode.emplace_back(flatMedianArrays.Data() + totalFlatArraySize, 0);
                        totalFlatArraySize += parentCount;
                    }

                    for (const Link& link: layers[i-1].m_downwardLinks)
                    {
                        eastl::span<float>& medianSpan = medianArrayPerNode[vertexLayerIndex[GetFlatArrayIndex(link.m_parent, passCount)]];
                        *medianSpan.end() = horizontalPositions[GetFlatArrayIndex(link.m_child, passCount)].m_center;
                        medianSpan = { medianSpan.begin(), medianSpan.end() + 1 };
                    }

                    repositionNodes(layers[i - 1]);
                }
            }
        }

        {
            const float horizontalOffset = horizontalPositions[GetFlatArrayIndex(layers.front().m_nodes.front().m_index, passCount)].m_center;
            for (HorizontalPosition& pos: horizontalPositions)
            {
                pos.m_center -= horizontalOffset;
            }
        }

        {
            KE_ZoneScoped("Draw graph");

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::BeginChild(
                "Passes graph",
                ImVec2(0, 0),
                true,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleVar(2);

            const ImVec2 offset = ImGui::GetCursorScreenPos() + relativeOffset;
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->ChannelsSplit(2);

            drawList->ChannelsSetCurrent(0);
            {
                KE_ZoneScoped("Draw links");
                for (u32 layerIndex = 0; layerIndex < layers.size(); layerIndex++)
                {
                    const Layer& layer = layers[layerIndex];
                    for (const Link& link : layer.m_downwardLinks)
                    {
                        const u32 hoIndex0 = GetFlatArrayIndex(link.m_parent, passCount);;
                        ImVec2 p0{
                            horizontalPositions[hoIndex0].m_center,
                            link.m_parent & fakeVertexFlag
                                ? float(layerIndex) * (nodeHeight + verticalSpacing)
                                : float(layerIndex) * (nodeHeight + verticalSpacing) + nodeHeight / 2.f};
                        p0 += offset;

                        const u32 hoIndex1 = GetFlatArrayIndex(link.m_child, passCount);;
                        ImVec2 p1{
                            horizontalPositions[hoIndex1].m_center,
                            link.m_child & fakeVertexFlag
                                ? float(layerIndex + 1) * (nodeHeight + verticalSpacing)
                                : float(layerIndex + 1) * (nodeHeight + verticalSpacing) - nodeHeight / 2.f};
                        p1 += offset;

                        drawList->AddBezierCubic(
                            p0,
                            p0 + ImVec2{0, verticalSpacing + (link.m_parent & fakeVertexFlag ? nodeHeight / 2.f : 0.f)},
                            p1 - ImVec2{0, verticalSpacing + (link.m_child & fakeVertexFlag ? nodeHeight / 2.f : 0.f)},
                            p1,
                            link.m_selected ? IM_COL32(255, 255, 128, 100) : IM_COL32(255, 255, 255, 40),
                            link.m_selected ? 4.f : 2.f);
                    }
                }
            }

            constexpr ImU32 renderPassColor = IM_COL32(60, 10, 10, 200);
            constexpr ImU32 computePassColor = IM_COL32(10, 10, 60, 200);
            constexpr ImU32 transferPassColor = IM_COL32(10, 60, 10, 200);

            {
                KE_ZoneScoped("Draw nodes");
                for (u32 i = 0; i < passCount; i++)
                {
                    if (layersIndices[i] == invalid)
                    {
                        continue;
                    }

                    ImGui::PushID(_builder.m_dag.data() + i);

                    drawList->ChannelsSetCurrent(1);
                    const ImVec2 rectMin =
                        offset
                        + ImVec2(
                            horizontalPositions[i].m_center - horizontalPositions[i].m_halfWidth,
                            (nodeHeight + verticalSpacing) * static_cast<float>(layersIndices[i]) - nodeHeight / 2.f);
                    ImGui::SetCursorScreenPos(rectMin + padding);
                    ImGui::BeginGroup();
                    ImGui::Text("%s", _builder.m_declaredPasses[i].m_name.m_string.c_str());
                    ImU32 color;
                    switch (_builder.m_declaredPasses[i].m_type)
                    {
                    case PassType::Render:
                        color = renderPassColor;
                        ImGui::Text("Render");
                        break;
                    case PassType::Compute:
                        color = computePassColor;
                        ImGui::Text("Compute");
                        break;
                    case PassType::Transfer:
                        color = transferPassColor;
                        ImGui::Text("Transfer");
                        break;
                    default:
                        KE_FATAL("Unsupported");
                    }
                    ImGui::EndGroup();
                    const ImVec2 rectMax = ImGui::GetItemRectSize() + rectMin + padding + padding;

                    drawList->ChannelsSetCurrent(0);
                    ImGui::SetCursorScreenPos(rectMin);
                    ImGui::InvisibleButton("##pass", rectMax - rectMin);
                    if (ImGui::IsItemActive())
                    {
                        selectedPass = _builder.m_declaredPasses[i].m_name.m_hash;
                    }

                    drawList->AddRectFilled(rectMin, rectMax, color, 4.f);
                    if (selectedPass == _builder.m_declaredPasses[i].m_name.m_hash)
                    {
                        drawList->AddRect(rectMin, rectMax, IM_COL32(255, 255, 255, 255), 4.f);
                    }

                    ImGui::PopID();
                }
            }
            drawList->ChannelsMerge();

            if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(1, 0.f))
            {
                relativeOffset += ImGui::GetIO().MouseDelta;
            }

            ImGui::EndChild();
        }
    }

    void ImGuiDebugWindow::DisplayBuilderResources(
        const Builder& _builder,
        const Registry& _registry,
        AllocatorInstance _tempAllocator)
    {
        KE_ZoneScopedFunction("ImGuiDebugWindow::DisplayBuilderPasses");

        static bool cullUnusedPasses = false;
        ImGui::Checkbox("Cull unused passes", &cullUnusedPasses);

        struct ResourceUse
        {
            union
            {
                const PassAttachmentDeclaration* m_attachmentDeclaration;
                const Dependency* m_dependency;
            };

            const PassDeclaration* m_pass;

            enum UseType {
                ReadDependency,
                WriteDependency,
                ColorAttachment,
                DepthStencilAttachment
            } m_useType;

            [[nodiscard]] bool IsDependency() const { return m_useType == ReadDependency || m_useType == WriteDependency; }
        };

        const char* useTypeNames[4] = {
            "read dependency",
            "write dependency",
            "color attachment",
            "depth-stencil attachment",
        };

        struct ResourceData {
            eastl::vector<ResourceUse> m_uses;
        };

        eastl::vector_map<SimplePoolHandle, ResourceData> resources(_tempAllocator);

        for (auto passIdx = 0; passIdx < _builder.m_declaredPasses.size(); passIdx++)
        {
            if (cullUnusedPasses && !_builder.m_passAlive[passIdx])
                continue;

            const PassDeclaration& pass = _builder.m_declaredPasses[passIdx];

            const auto processDependency = [&](const Dependency& _dependency, const ResourceUse::UseType _useType)
            {
                const SimplePoolHandle resource = _registry.GetUnderlyingResource(_dependency.m_resource);

                auto it = resources.find(resource);
                if (it == resources.end())
                {
                    it = resources.emplace(
                        resource,
                        ResourceData { .m_uses { _tempAllocator } }).first;
                }

                it->second.m_uses.push_back(ResourceUse {
                    .m_dependency = &_dependency,
                    .m_pass = &pass,
                    .m_useType = _useType,
                });
            };

            for (const Dependency& dependency: pass.m_readDependencies)
                processDependency(dependency, ResourceUse::ReadDependency);
            for (const Dependency& dependency: pass.m_writeDependencies)
                processDependency(dependency, ResourceUse::WriteDependency);

            const auto processAttachment = [&](const PassAttachmentDeclaration& _attachment, const ResourceUse::UseType _useType)
            {
                const SimplePoolHandle resource = _registry.GetUnderlyingResource(_attachment.m_rtv);

                auto it = resources.find(resource);
                if (it == resources.end())
                {
                    it = resources.emplace(
                        resource,
                        ResourceData { .m_uses { _tempAllocator } }).first;
                }

                it->second.m_uses.push_back(ResourceUse {
                    .m_attachmentDeclaration = &_attachment,
                    .m_pass = &pass,
                    .m_useType = _useType,
                });
            };

            for (const PassAttachmentDeclaration& attachment : pass.m_colorAttachments)
                processAttachment(attachment, ResourceUse::ColorAttachment);
            if (pass.m_depthAttachment.has_value())
                processAttachment(pass.m_depthAttachment.value(), ResourceUse::DepthStencilAttachment);
        }

        for (const auto& [resourceHandle, resourceData]: resources)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);

            const Resource& resource = _registry.GetResource(resourceHandle);

#if !defined(KE_FINAL)
            const char* name = resource.m_name.c_str();
#else
            char name[64];
            snprintf(name, sizeof(name), "0x%16p", &resource);
#endif

            if (!ImGui::TreeNode(&resource, "#%zu %s", resourceHandle, name))
                continue;

            for (const ResourceUse& resourceUse: resourceData.m_uses)
            {
                const SimplePoolHandle trueResourceHandle = resourceUse.IsDependency()
                    ? resourceUse.m_dependency->m_resource
                    : resourceUse.m_attachmentDeclaration->m_rtv;
                if (trueResourceHandle != resourceHandle)
                {
                    const Resource& trueResource = _registry.GetResource(trueResourceHandle);

#if !defined(KE_FINAL)
                    const char* trueName = trueResource.m_name.empty() ? nullptr : trueResource.m_name.c_str();
#else
                    const char* trueName = nullptr;
#endif

                    const char* type;
                    switch (trueResource.m_type)
                    {
                    case ResourceType::TextureView:
                        type = "texture view";
                        break;
                    case ResourceType::BufferView:
                        type = "buffer view";
                        break;
                    case ResourceType::RenderTargetView:
                        type = "render target view";
                        break;
                    default:
                        type = "unknown";
                    }

                    if (trueName == nullptr)
                    {
                        ImGui::Text(
                            "Used in pass '%s' as a %s (%s #%zu)",
                            resourceUse.m_pass->m_name.m_string.c_str(),
                            useTypeNames[resourceUse.m_useType],
                            type,
                            trueResourceHandle);
                    }
                    else
                    {
                        ImGui::Text(
                            "Used in pass '%s' as a %s (%s #%zu named '%s')",
                            resourceUse.m_pass->m_name.m_string.c_str(),
                            useTypeNames[resourceUse.m_useType],
                            type,
                            trueResourceHandle,
                            trueName);
                    }
                }
                else
                {
                    ImGui::Text(
                        "Used in pass '%s' as a %s",
                        resourceUse.m_pass->m_name.m_string.c_str(),
                        useTypeNames[resourceUse.m_useType]);
                }
            }

            ImGui::TreePop();
        }
    }
}