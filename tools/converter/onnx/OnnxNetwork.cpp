// Copyright 2019 MAI. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fcntl.h>
#include <fstream>
#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "source/ops/cpu/CPURegister.h"
#include "OnnxNetwork.h"
#include "Type.h"
#include "source/core/Allocator.h"
#include "source/core/OperatorRegister.h"
#include "source/util/MAIUtil.h"
#include "onnx.pb.h"

namespace MAI {
std::string& trim(std::string &s)
{
    if (s.empty())
    {
        return s;
    }

    s.erase(0,s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    return s;
}

std::vector<std::string> split(const std::string &s, const std::string &seperator){
    std::vector<std::string> result;
    typedef std::string::size_type string_size;
    string_size i = 0;

    while(i != s.size()){
        int flag = 0;
        while(i != s.size() && flag == 0){
            flag = 1;
            for(string_size x = 0; x < seperator.size(); ++x) {
                if(s[i] == seperator[x]){
                    ++i;
                    flag = 0;
                    break;
                }
            }
        }

        flag = 0;
        string_size j = i;
        while(j != s.size() && flag == 0){
            for(string_size x = 0; x < seperator.size(); ++x) {
                if(s[j] == seperator[x]){
                    flag = 1;
                    break;
                }
            }
            if(flag == 0)
                ++j;
        }
        if(i != j){
            std::string v = s.substr(i, j-i);
            trim(v);
            if (v != "") {
                result.push_back(v);
            }
            i = j;
        }
    }
    return result;
}

static DataType onnx2MIDataType(int32 dataType) {
    onnx::TensorProto::DataType onnxDataType = (onnx::TensorProto::DataType)dataType;
    static std::map<onnx::TensorProto::DataType, DataType> maps =
    {
        {onnx::TensorProto::UNDEFINED, DT_INVALID},
        {onnx::TensorProto::FLOAT, DT_FLOAT},
        {onnx::TensorProto::UINT8, DT_UINT8},
        {onnx::TensorProto::INT8, DT_INT8},
        {onnx::TensorProto::UINT16, DT_UINT16},
        {onnx::TensorProto::INT16, DT_INT16},
        {onnx::TensorProto::INT32, DT_INT32},
        {onnx::TensorProto::INT64, DT_INT64},
        {onnx::TensorProto::STRING, DT_STRING},
        {onnx::TensorProto::BOOL, DT_BOOL},
    };

    return maps[onnxDataType];
}

class OnnxParser;
typedef std::function<void(OnnxParser&, const onnx::GraphProto&, const onnx::NodeProto&)> OpParserFunction;
static std::map<std::string, OpParserFunction> opParsers;

bool registerOpParser(OpParserFunction functionParser, const std::string& names) {
    std::vector<std::string> nameVector = split(names, ",");
    bool r = false;
    for (const std::string& name : nameVector) {
        if (opParsers.find(name) == opParsers.end()) {
            opParsers[name] = functionParser;
        } else {
            r = true;
        }

    }
    return r;
}

static const void* getTensorData(const onnx::TensorProto& tensorProto) {
    if (tensorProto.name() == "reshape__254__255") {
        ALOGI("shape getData, dataType:%s", getNameFromDataType(onnx2MIDataType(tensorProto.data_type())).c_str());
        const int64* data = reinterpret_cast<const int64*>(tensorProto.raw_data().c_str());
        ALOGI("data0:%d, 1:%d", data[0], data[1]);
    }
#define FIND_TENSOR_DATA(DATA_TYPE, dataFunc) \
    if (tensorProto.data_type() == onnx::TensorProto::DATA_TYPE) { \
        if (tensorProto.dataFunc##_data().size() != 0) { \
            return tensorProto.dataFunc##_data().data(); \
        } else { \
            return tensorProto.raw_data().c_str(); \
        } \
    }

    FIND_TENSOR_DATA(FLOAT, float);
    FIND_TENSOR_DATA(UINT8, int32);
    FIND_TENSOR_DATA(INT32, int32);
    FIND_TENSOR_DATA(INT8, int32);
    FIND_TENSOR_DATA(UINT16, int32);
    FIND_TENSOR_DATA(INT16, int32);
    FIND_TENSOR_DATA(UINT8, int32);
    FIND_TENSOR_DATA(BOOL, int32);
    // TODO:(gavinchen) how to store float16 using int32
    //FIND_TENSOR_DATA(FLOAT16, int32);
    FIND_TENSOR_DATA(INT64, int64);
#undef FIND_TENSOR_DATA
    if (tensorProto.data_type() == onnx::TensorProto::STRING) {
        MAI_ABORT("Unimplement to parse string tensor");
    }
    return NULL;
}

class OnnxParser {
public:
    OnnxParser(OnnxNetwork* network) : mOnnxNetwork(network) {
    }

    void parse(const std::string& netPath) {
        if (!openGraph(netPath)) {
            return;
        }
        ALOGI("Model value_infos:%d", mOnnxGraphProto.value_info_size());
        ALOGI("Model inputs:%d", mOnnxGraphProto.input_size());
        ALOGI("Model initializer:%d", mOnnxGraphProto.initializer_size());
        ALOGI("Model outputs:%d, %s", mOnnxGraphProto.output_size(), mOnnxGraphProto.output(0).name().c_str());
        std::vector<std::string> modelConstInputs(mOnnxGraphProto.initializer_size());
        // 1. parse model const tensor
        for (int32 i = 0; i < mOnnxGraphProto.initializer_size(); ++i) {
            auto& tensorProto = mOnnxGraphProto.initializer(i);
            modelConstInputs[i] = tensorProto.name();
            std::unique_ptr<Tensor> tensor(new Tensor(onnx2MIDataType(tensorProto.data_type()), new CPUAllocator()));
            tensor->setName(tensorProto.name());
            tensor->setDataFormat(OIHW);// TODO:(gavinchen) setDataFormat should be done in node parser
            //ALOGI("addTensor:%s", tensorProto.name().c_str());
            std::vector<shape_t> tensorShape(tensorProto.dims().begin(), tensorProto.dims().end());
            tensor->allocateBuffer(tensorShape);
            const void* tensorData = getTensorData(tensorProto);
            MAI_CHECK_NULL(tensorData);
            tensor->copy(tensorData, tensor->size());
            mOnnxNetwork->addTensor(tensor);
        }
        // 2. parse model input & output
        for (int32 i = 0; i < mOnnxGraphProto.input_size(); ++i) {
            auto& input = mOnnxGraphProto.input(i);
            if (std::find(modelConstInputs.begin(), modelConstInputs.end(), input.name()) == modelConstInputs.end()) {// This is a placeholder
                const onnx::TypeProto_Tensor& tensor = input.type().tensor_type();
                std::vector<shape_t> inputShape(tensor.shape().dim_size());
                for (int32 d = 0; d < tensor.shape().dim_size(); ++d) {
                    int32 dim = tensor.shape().dim(d).dim_value();
                    inputShape[d] = (dim == 0 || dim == -1) ? 1 : dim; //TODO:(gavinchen) receive input size from command line
                }
                mOnnxNetwork->addModelInput(input.name(),
                        onnx2MIDataType(tensor.elem_type()), inputShape);
            }
        }
        // 3. parse operator
        ALOGI("op count:%d", mOnnxGraphProto.node_size());
        int nodeCount = mOnnxGraphProto.node_size();
        for (int32 i = 0; i < nodeCount; ++i) {
            const onnx::NodeProto& node = mOnnxGraphProto.node(i);
            const std::string& opType = node.op_type();
            if (opParsers.find(opType) != opParsers.end()) {
                opParsers[opType](*this, mOnnxGraphProto, node);
            } else {
                ALOGE("Unrecognize op:%s", opType.c_str());
            }
        }
    }

private:
    bool openGraph(const std::string& netPath) {
        std::ifstream ifs(netPath, std::ifstream::in | std::ifstream::binary);
        if (!ifs.is_open()) {
            ALOGE("Cannot open graph:%s", netPath.c_str());
            return false;
        }
        google::protobuf::io::IstreamInputStream input(&ifs);
        google::protobuf::io::CodedInputStream codedStream(&input);
        bool success = mOnnxModelProto.ParseFromCodedStream(&codedStream);
        if (!success) {
            ALOGE("Cannot parse graph:%s", netPath.c_str());
            return false;
        }

        ifs.close();
        mOnnxGraphProto = mOnnxModelProto.graph();
        return success;
    }

public:
    OnnxNetwork* mOnnxNetwork;
    onnx::ModelProto mOnnxModelProto;
    onnx::GraphProto mOnnxGraphProto;
    std::map<std::string, int> mOpMap;
};

class OpParserBase {
public:
    static void processArgType() {
    }

    static std::unique_ptr<Operator> createOperator(MAIOperator opType, DataType dataType) {
        std::unique_ptr<Operator> op = OperatorRegister::getInstance()->createOperator({opType, dataType});
        return op;
    }

    static PaddingMode onnx2MIPaddingMode(const std::string& paddingModeStr) {
        if (paddingModeStr == "SAME_LOWER") {
            MAI_CHECK("Unsupport padding mode:%s", paddingModeStr.c_str());
            return SAME;
        }
        if (paddingModeStr == "SAME_UPPER") {
            MAI_CHECK("Unsupport padding mode:%s", paddingModeStr.c_str());
            return SAME;
        }
        if (paddingModeStr == "NOTSET") {
            return INVALID;
        }
        if (paddingModeStr == "VALID") {
            return VALID;
        }
        return INVALID;
    }
};
static bool findTensor(const onnx::GraphProto& graphProto, const std::string& name, onnx::TensorProto& tensor) {
    auto& tensors = graphProto.initializer();
    for (int32 i = 0; i < graphProto.initializer_size(); ++i) {
        auto& tmpTensor = graphProto.initializer(i);
        //ALOGI("Tensor:%s", tmpTensor.name().c_str());
        if (tmpTensor.name() == name) {
            tensor = tmpTensor;
            return true;
        }
    }
    return false;
}

static onnx::TensorProto::DataType findOpDataType(const onnx::GraphProto& graphProto, const std::string& name) {
    onnx::TensorProto::DataType onnxDataType;
    onnx::TensorProto tensor;
    bool r = findTensor(graphProto, name, tensor);
    if (r) {
        onnxDataType = (onnx::TensorProto::DataType)tensor.data_type();
    } else {
        MAI_CHECK(false, "Cannot find tensor:%s", name.c_str());
    }
    return onnxDataType;
}

static void parseAttrs(OnnxParser& parser, const onnx::NodeProto& node, MAIOperator opType, onnx::TensorProto::DataType& onnxDataType,
        Param* param, std::map<std::string, std::vector<std::function<void(const onnx::AttributeProto&)>>> attrParsers) {
    std::map<std::string, bool> parserUsed;
    for (auto it = attrParsers.begin(); it != attrParsers.end(); ++it) {
        parserUsed[it->first] = false;
        MAI_CHECK((it->second.size() == 1 || it->second.size() == 2),
                "attrParsers cannot support functions size 1 or 2 but not: %d", it->second.size());
    }
    auto& attrs = node.attribute();
    for (auto it = attrs.begin(); it != attrs.end(); ++it) {
        auto attrParserIt = attrParsers.find((*it).name());
        if (attrParserIt != attrParsers.end()) {
            attrParserIt->second[0](*(it));
            parserUsed[(*it).name()] = true;
        }
    }

    // call default attrs parser
    for (auto it = parserUsed.begin(); it != parserUsed.end(); ++it) {
        if (it->second == false && attrParsers[it->first].size() == 2) {
            const onnx::AttributeProto proto;
            attrParsers[it->first][1](proto);
        }
    }

    std::unique_ptr<Operator> op = OpParserBase::createOperator(opType, onnx2MIDataType(onnxDataType));
    if (param != NULL) {
        op->setParam(param);
    }
    op->setName(node.name());
    for(int32 i = 0; i < node.input_size(); ++i) {
        //ALOGI("name:%s input:%s", node.name().c_str(), node.input(i).c_str());
        op->addInputName(node.input(i));
    }
    for(int32 i = 0; i < node.output_size(); ++i) {
        op->addOutputName(node.output(i));
        std::unique_ptr<Tensor> tensor(new Tensor(onnx2MIDataType(onnxDataType), new CPUAllocator()));
        tensor->setName(node.output(i));
        tensor->setDataFormat(NCHW);
        //ALOGI("add output tensor:%s", tensor->name().c_str());
        parser.mOnnxNetwork->addTensor(tensor);
    }
    parser.mOnnxNetwork->addOperator(op);
}


#define OP_PARSER_NAME(_1, ...) _1

#define OP_PARSER(...) \
    class OP_PARSER_NAME(__VA_ARGS__) : public OpParserBase { \
    private: \
        static void parse(OnnxParser& parser, const onnx::GraphProto& graph, const onnx::NodeProto& node); \
        static int const result; \
    }; \
    int const OP_PARSER_NAME(__VA_ARGS__)::result = registerOpParser(OP_PARSER_NAME(__VA_ARGS__)::parse, #__VA_ARGS__); \
    void OP_PARSER_NAME(__VA_ARGS__)::parse(OnnxParser& parser, const onnx::GraphProto& graph, const onnx::NodeProto& node)

#define MULTI_OP_PARSER(...) \

OP_PARSER(Conv) {
    //TODO:(gavinchen)
    bool isDepthwiseConv = false;
    auto& attrs = node.attribute();
    for (auto it = attrs.begin(); it != attrs.end(); ++it) {
        if ((*it).name() == "group") {
            if ((*it).i() > 0) {
                //TODO: check group size is equal to input channel size
                isDepthwiseConv = true;
            }
        }
    }

    if (isDepthwiseConv) {
        parser.mOnnxNetwork->getTensor(node.input(1))->setDataFormat(IOHW);
    }

    onnx::TensorProto::DataType onnxDataType = onnx::TensorProto::FLOAT;
    Conv2DParam* param = isDepthwiseConv ? new DepthwiseConv2dParam() : new Conv2DParam();
    std::map<std::string, std::vector<std::function<void(const onnx::AttributeProto&)>>> attrParsers = {
        {"strides",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    if (attr.ints().size() != 2) {
                        MAI_ABORT("Unimplement strides size:%d", attr.ints().size());
                    }
                    // NCHW
                    param->strides.push_back(1);// N
                    param->strides.push_back(1);// C
                    param->strides.insert(param->strides.end(), attr.ints().begin(), attr.ints().end()); // HW
                }
            }
        },

        {"dilations",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    if (attr.ints().size() != 2) {
                        MAI_ABORT("Unimplement dilations size:%d", attr.ints().size());
                    }
                    param->dilations.push_back(1); // N
                    param->dilations.push_back(1); // C
                    param->dilations.insert(param->dilations.end(), attr.ints().begin(), attr.ints().end());
                }
            }
        },

        {"pads",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    //ONNX pads: TOP LEFT BOTTOM RIGHT
                    //MAI pads: TOP BOTTOM LEFT RIGHT
                    MAI_CHECK(attr.ints().size() == 4, "Unimplement pads size:%d", attr.ints().size());
                    param->paddings.resize(4);
                    param->paddings[0] = attr.ints(0);
                    param->paddings[1] = attr.ints(2);
                    param->paddings[2] = attr.ints(1);
                    param->paddings[3] = attr.ints(3);
                }
            }
        },

        {"auto_pad",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    //TODO:(gavinchen) deprecated
                    MAI_ABORT("auto_pad is deprecated");
                }
            }
        },
    };

    if (isDepthwiseConv) {
        parseAttrs(parser, node, DEPTHWISE_CONV2D, onnxDataType, param, attrParsers);
    } else {
        parseAttrs(parser, node, CONV2D, onnxDataType, param, attrParsers);
    }
}

OP_PARSER(DepthwiseConv2dNative) {
    DepthwiseConv2dParam* param = new DepthwiseConv2dParam();
}

OP_PARSER(AveragePool) {
    PoolParam* param = new PoolParam();
    onnx::TensorProto::DataType onnxDataType = onnx::TensorProto::FLOAT;
    std::map<std::string, std::vector<std::function<void(const onnx::AttributeProto&)>>> attrParsers = {
        {"auto_pad",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    param->paddingMode = onnx2MIPaddingMode(attr.s());
                }
            }
        },

        {"count_include_pad",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    //TODO:(gavinchen)
                }
            }
        },

        {"kernel_shape",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    if (attr.ints_size() < 4) {
                        for (int32 i = 0; i < (4 - attr.ints_size()); ++i) {
                            param->kernelSizes.emplace_back(1);
                        }
                    }
                    param->kernelSizes.insert(param->kernelSizes.end(), attr.ints().begin(), attr.ints().end());
                }
            }
        },

        {"pads",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    param->paddings.insert(param->paddings.end(), attr.ints().begin(), attr.ints().end());
                },

                [&param](const onnx::AttributeProto& attr)
                {

                    param->paddings.resize(4);
                    std::fill_n(param->paddings.begin(), 4, 0);// 0 0 0 0
                },
            }
        },

        {"strides",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    if (attr.ints().size() != 2) {
                        MAI_ABORT("Unimplement strides size:%d", attr.ints().size());
                    }
                    // NCHW
                    param->strides.push_back(1);// N
                    param->strides.push_back(1);// C
                    param->strides.insert(param->strides.end(), attr.ints().begin(), attr.ints().end());
                }
            }
        },
    };
    parseAttrs(parser, node, AVG_POOL, onnxDataType, param, attrParsers);
}

OP_PARSER(BatchNormalization) {
    FusedBatchNormParam* param = new FusedBatchNormParam();
    onnx::TensorProto::DataType onnxDataType = onnx::TensorProto::FLOAT;
    std::map<std::string, std::vector<std::function<void(const onnx::AttributeProto&)>>> attrParsers = {
        {"epsilon",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    param->epsilon = attr.f();
                }
            }
        },

        {"momentum",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    //TODO:(gavinchen)
                }
            }
        },

        {"spatial",
            {
                [](const onnx::AttributeProto& attr)
                {
                    //TODO:(gavinchen)
                }
            }
        },
    };
    parseAttrs(parser, node, FUSED_BATCH_NORM, onnxDataType, param, attrParsers);
}

OP_PARSER(Clip) {
    onnx::TensorProto::DataType onnxDataType = onnx::TensorProto::FLOAT;
    float min = std::numeric_limits<float>::lowest();
    float max = std::numeric_limits<float>::max();
    std::map<std::string, std::vector<std::function<void(const onnx::AttributeProto&)>>> attrParsers = {
        {"min",
            {
                [&min](const onnx::AttributeProto& attr)
                {
                    min = attr.f();
                }
            }
        },

        {"max",
            {
                [&max](const onnx::AttributeProto& attr)
                {
                    max = attr.f();
                }
            }
        },
    };
    parseAttrs(parser, node, RELU6, onnxDataType, NULL/*param*/, attrParsers);
}

OP_PARSER(Squeeze) {
    SqueezeParam* param = new SqueezeParam();
    onnx::TensorProto::DataType onnxDataType = onnx::TensorProto::FLOAT;
    std::map<std::string, std::vector<std::function<void(const onnx::AttributeProto&)>>> attrParsers = {
        {"axes",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    param->squeezeDims.insert(param->squeezeDims.end(), attr.ints().begin(), attr.ints().end());
                }
            }
        },
    };
    parseAttrs(parser, node, SQUEEZE, onnxDataType, param, attrParsers);
}

OP_PARSER(Reshape) {
    onnx::TensorProto::DataType onnxDataType = onnx::TensorProto::FLOAT;
    std::map<std::string, std::vector<std::function<void(const onnx::AttributeProto&)>>> attrParsers;
    parseAttrs(parser, node, RESHAPE, onnxDataType, NULL/*param*/, attrParsers);
}

OP_PARSER(Shape) {
    onnx::TensorProto::DataType onnxDataType = onnx::TensorProto::INT64;
    std::map<std::string, std::vector<std::function<void(const onnx::AttributeProto&)>>> attrParsers;
    parseAttrs(parser, node, SHAPE, onnxDataType, NULL/*param*/, attrParsers);
}

OP_PARSER(Softmax) {
    SoftmaxParam* param = new SoftmaxParam();
    param->beta = 1.f;
    param->axis = -1;
    onnx::TensorProto::DataType onnxDataType = onnx::TensorProto::FLOAT;
    std::map<std::string, std::vector<std::function<void(const onnx::AttributeProto&)>>> attrParsers = {
        {"axis",
            {
                [&param](const onnx::AttributeProto& attr)
                {
                    param->axis = attr.i();
                }
            }
        },
    };
    parseAttrs(parser, node, SOFTMAX, onnxDataType, param, attrParsers);
}

OP_PARSER(Cast) {
    onnx::TensorProto::DataType onnxDataType;
    std::map<std::string, std::vector<std::function<void(const onnx::AttributeProto&)>>> attrParsers = {
        {"to",
            {
                [&](const onnx::AttributeProto& attr)
                {
                    onnxDataType = (onnx::TensorProto::DataType)attr.i();
                }
            }
        },
    };
    parseAttrs(parser, node, CAST, onnxDataType, NULL/*param*/, attrParsers);
}

OnnxNetwork::OnnxNetwork(const std::string& netPath) {
    Op::CPU::CPURegister cpuRegister;
    OnnxParser parser(this);
    parser.parse(netPath);
}

MAI_STATUS OnnxNetwork::init() {
    ALOGI("init ");
    for (auto it = mOperators.begin(); it != mOperators.end(); ++it) {
        (*it)->init();
    }
    return MAI_SUCCESS;
}

MAI_STATUS OnnxNetwork::run() {
    ALOGI("run relu6 is exists:%d", mTensors.find("MobilenetV1/MobilenetV1/Conv2d_0/Relu6:0") != mTensors.end());
    for (auto it = mOperators.begin(); it != mOperators.end(); ++it) {
        ALOGI("run op:%s", (*it)->name().c_str());
        (*it)->run();
    }

#if 1
    // tensor to file
    const std::string kOutputDir = "output";
    for(int32 i = 0; i < mModelInputs.size(); ++i) {
        getTensor(mModelInputs[i])->toFile(kOutputDir);
    }
    for (auto it = mOperators.begin(); it != mOperators.end(); ++it) {
        for (int32 i = 0; i < (*it)->outputNames().size(); ++i) {
            (*it)->getOutputTensor(i)->toFile(kOutputDir);
        }
    }
#endif
    return MAI_SUCCESS;
}

MAI_STATUS OnnxNetwork::addOperator(std::unique_ptr<Operator>& op) {
    op->setNeuralNetwork(this);
    mOperators.emplace_back(std::move(op));
    return MAI_SUCCESS;
}

MAI_STATUS OnnxNetwork::addTensor(std::unique_ptr<Tensor>& tensor) {
    MAI_CHECK(mTensors.find(tensor->name()) == mTensors.end(), "%s has exists", tensor->name().c_str());
    mTensors.emplace(tensor->name(), std::move(tensor));
    return MAI_SUCCESS;
}

Tensor* OnnxNetwork::getTensor(const std::string& name) {
    return mTensors[name].get();
}

Operator* OnnxNetwork::getOperator(const std::string& name) {
    for (auto it = mOperators.begin(); it != mOperators.end(); ++it) {
        if ((*it)->name() == name) {
            return it->get();
        }
    }
    return NULL;
}

void OnnxNetwork::addModelInput(const std::string& inputName,
        DataType dataType,
        const std::vector<shape_t>& inputShape) {
    mModelInputs.emplace_back(inputName);
    std::unique_ptr<Tensor> tensor(new Tensor(dataType, new CPUAllocator()));
    tensor->setDataFormat(NCHW);
    tensor->setName(inputName);
    tensor->allocateBuffer(inputShape);
    ALOGI("add model input tensor: %s", tensor->name().c_str());
    addTensor(tensor);
}

void OnnxNetwork::addModelOutput(const std::string& outputName) {
    mModelOutputs.emplace_back(outputName);
}

void OnnxNetwork::builGraph() {
}

} // namespace MAI
