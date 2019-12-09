#include "ann.hpp"

#include "utils.cpp"

#include <iostream>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unistd.h>

ANN::ANN(const std::initializer_list<uint>& n_perceptrons){
    auto it=n_perceptrons.begin();
    m_n_input=*n_perceptrons.begin();
    m_n_output=*(n_perceptrons.end()-1);
    m_batchSize=1;
    for(auto it=n_perceptrons.begin();it!=n_perceptrons.end();++it){
        Layer layer;
        layer.add_perceptron(*it);
        if(it+1!=n_perceptrons.end()) layer.weights().set_shape(*(it+1), *it);
        layer.weights().random_init(-1., 1.);
        m_layers.push_back(layer);
    }
}

ANN::~ANN(){}

void ANN::set_labels(LayerType label_type, const std::initializer_list<std::string>& labels){
    uint index;
    switch (label_type)
    {
    case InputLayer:
        index=0;
        break;
    case OutputLayer:
        index=m_layers.size()-1;
        break;
    default: break;
    }
    m_layers[index].set_labels(labels);
}

void ANN::set_dataSet(DataSet& data_set){
    m_dataSet=&data_set;
}

void ANN::set_batchSize(BatchSize batch_size){
    if(!m_dataSet){
        std::cout<<"ERROR [set_batchsize()]: No available dataset!\n";
    }
    
    Shape shape=m_dataSet->shape();
    switch (batch_size)
    {
    case None:
        ;
        break;
    case Half:
        m_batchSize=shape.n_row/2;
        break;
    case Quarter:
        m_batchSize=shape.n_row/4;
        break;
    default: break;
    }
}

void ANN::set_batchSize(uint batch_size){
    m_batchSize=batch_size;
}

void ANN::add_layer(const Layer& layer){
    m_layers.push_back(layer);
}

void ANN::add_layer(uint n_layer, const std::initializer_list<uint>& n_perceptrons){
    for(uint i=0;i<n_layer;++i){
    Layer layer;
    layer.add_perceptron(*(n_perceptrons.begin()+i));
    m_layers.push_back(layer);
    }
}

void ANN::insert_layer(uint index, const Layer& layer){
    m_layers.insert(m_layers.begin()+index, layer);
}

void ANN::insert_layer(uint index, uint n_perceptrons){
    Layer layer;
    layer.add_perceptron(n_perceptrons);
    insert_layer(index, layer);
}

void ANN::insert_layer(const std::initializer_list<uint>& indexes, const std::initializer_list<uint>& n_perceptrons){
    if(indexes.size()==n_perceptrons.size()){
        auto index_it=indexes.begin();
        auto percept_it=n_perceptrons.begin();

        for(;index_it!=indexes.end();++index_it, ++percept_it){
            insert_layer(*index_it, *percept_it);
        }
    }
}

void ANN::insert_layer(const std::initializer_list<uint>& n_perceptrons){
    uint i=0;
    for(auto it=n_perceptrons.begin();it!=n_perceptrons.end();++it){
        insert_layer(i++, *it);
    }
}

void ANN::initialize(double beg, double end){
    srand(time(0));
    for(uint i=0;i<m_layers.size()-1;++i){
        m_layers[i].weights().random_init(beg, end);
    }
}

void ANN::train(DataSet& dataset, uint n_epoch, uint batch_size){
    m_dataSet=&dataset;
    double error;
    if(batch_size==0) m_batchSize=m_dataSet->shape().n_row;
    m_batchSize=batch_size;
    uint n_groups=m_dataSet->shape().n_row/m_batchSize;

    for(uint i=0;i<n_epoch;++i){
        for(uint j=0;j<n_groups;++j){
            for(uint k=0;k<m_batchSize;++k){
                Matrix error=feed_forward(m_dataSet->get_input(j*m_batchSize+k))-DataSet::one_hot_encode(m_dataSet->get_output(j*m_batchSize+k), 2);
                // std::cout<<"Error: "<<sqrt(pow(error[0][0], 2)+pow(error[0][0], 2))<<'\n';
                // sleep(0.3);
                back_propagate(error);
            }
        }
        if(i%10==0) m_lr/=1.5;
    }
}

void ANN::train(uint n_epoch, uint batch_size){
    train(*m_dataSet, n_epoch, batch_size);
}

std::string ANN::predict(const std::vector<double>& inputs){
    Matrix output=feed_forward(inputs);
    double max=0;
    uint index=0;

    for(uint i=0;i<output.shape().n_col;++i){
        if(max<output[0][i]){
            max=output[0][i];
            index=i;
        }
    }

    std::cout<<"Guessing: "<<output<<'\n';

    return m_layers[m_layers.size()-1].get_perceptron(index).label();
}

double ANN::accuracy(const DataSet& dataSet){
    uint right_guesses=0;
    uint all_guesses=dataSet.shape().n_row;

    for(uint i=0;i<all_guesses;++i){
        if(is_same(feed_forward(dataSet.get_input(i))[0], DataSet::one_hot_encode(dataSet.get_output(i), 2)[0]))
            ++right_guesses;
    }

    return (double)right_guesses/all_guesses;
}

/*
 * Weight file structure: 
 * -----------------------
 * # of layers
 * # of perceptrons in layer 1
 * # of weights in perceptron 1 of the current layer (layer 1)
 * weights
*/

void ANN::save(const std::string& filepath) const{
    uint n_perceptron=0, n_weights=0;
    std::ofstream file(filepath);

    // file<<m_layers.size()-1<<"\n";
    // for(uint i=0;i<m_layers.size()-1;++i){
    //     n_perceptron=m_layers[i].get_nb_perceptrons();
    //     file<<n_perceptron<<"\n";
    //     n_weights=m_layers[i+1].get_nb_perceptrons();
    //     file<<n_weights<<"\n";
    //     for(uint j=0;j<n_perceptron;++j){
    //         m_layers[i].get_perceptron(j).get_weights().print_weights(file);
    //         file<<'\n';
    //     }
    // }

    file.close();
}

void ANN::load(const std::string& filepath){
    std::stringstream stream;
    std::string line;
    uint n_perceptron, n_layer, n_weights;
    Matrix weight;

    std::ifstream file(filepath);

    // if(!file){
    //     std::cout<<"ERROR [weight file]: Couldn't load weights!\n";
    // }

    // std::getline(file, line);
    // n_layer=std::stoi(line);

    // if(n_layer!=m_layers.size()-1){
    //     std::cout<<"ERROR [weight loading]: Inaccurate # of layers!\n";
    // }

    // for(uint i=0;i<n_layer;++i){
    //     // std::cout<<"layer "<<i+1<<'\n';

    //     std::getline(file, line);
    //     n_perceptron=std::stoi(line);
    //     std::getline(file, line);
    //     n_weights=std::stoi(line);
    //     weight.set_shape(n_weights, 1);
        
    //     if(n_perceptron!=m_layers[i].get_nb_perceptrons()){
    //         std::cout<<"ERROR [weight loading]: Inaccurate # of perceptrons of layer "<<i+1<<'\n';
    //     }

    //     for(uint j=0;j<n_perceptron;++j){
    //         std::getline(file, line);
    //         stream.clear();
    //         stream<<line;
    //         for(uint k=0;k<n_weights;++k){
    //             std::getline(stream, line, ',');
    //             weight.set(k, 0)=std::stold(line);
    //         }
    //         m_layers[i].weights()=weight;
    //     }
    // }

    file.close();
}

void ANN::print(uint index) const{
    Perceptron perceptron;
    for(uint i=0;i<m_layers[index].get_nb_perceptrons();++i){
        const Perceptron perceptron=m_layers[index].get_perceptron(i);
        std::cout<<m_layers[index].label(i)<<": "<<perceptron.get_output()<<'\n';
    }
}

void ANN::print(LayerType type) const{
    uint index;
    switch (type)
    {
    case InputLayer:
        index=0;
        break;
    case OutputLayer:
        index=m_layers.size()-1;
        break;
    default: break;
    }
    print(index);
}

void ANN::print_structure() const{
    uint last_index=m_layers.size()-1;
    std::cout<<" ||| Structure:\n";
    std::cout<<" > Number of layers: "<<last_index+1<<" |-";
    for(uint i=0;i<=last_index;++i){
        std::cout<<m_layers[i].get_nb_perceptrons()<<"-";
    }   std::cout<<"|\n";
    std::cout<<" > Shapes of weights: ";
    for(uint i=0;i<m_layers.size()-1;++i){
        std::cout<<m_layers[i].weights().shape().n_row<<"x"<<m_layers[i].weights().shape().n_col<<" | ";
    }   std::cout<<'\n';
    std::cout<<" > Input labels:\n\t";
    for(uint i=0;i<m_layers[0].get_nb_perceptrons();++i){
        std::cout<<m_layers[0].label(i);
        if(i!=m_layers[0].get_nb_perceptrons()-1 &&
           !m_layers[0].label(i+1).empty()) std::cout<<"\n\t";
    }
    std::cout<<"\n > Output labels:\n\t";
    for(uint i=0;i<m_layers[last_index].get_nb_perceptrons();++i){
        std::cout<<m_layers[last_index].label(i);
        if(i!=m_layers[last_index].get_nb_perceptrons()-1) std::cout<<"\n\t";
    }
    std::cout<<"\n > Batch size: ";
    if(m_batchSize!=-1) std::cout<<m_batchSize;
    else    std::cout<<"none";
    std::cout<<"\n > Learning rate: "<<m_lr;
    if(m_dataSet){
        std::cout<<"\n > Dataset already exists (First 5 rows):\n";
        m_dataSet->print(5);
    }
    else{
        std::cout<<"\n > Dataset doesn't exist!\n";
    }
    std::cout<<"\n= = = = = = = = = = = = = =\n";
}

double ANN::net_error(const Matrix& error) const{
    double err=0;

    for(uint i=0;i<error.shape().n_col;++i){
        // std::cout<<": "<<error[0][i]<<" :";
        err+=0.5*pow(error[0][i], 2);
    }   // std::cout<<'\n';

    return err;
}

Matrix ANN::feed_forward(const std::vector<double>& inputs){
    Matrix mat;
    m_layers[0].equalize(inputs);
    for(uint i=1;i<m_layers.size();++i){
        mat=m_layers[i-1].get()*Matrix::transpose(m_layers[i-1].weights());
        m_layers[i].equalize(mat[0]);
    }
    return m_layers[m_layers.size()-1].get();
}

void ANN::back_propagate(const Matrix& error, uint layer_index){
    Matrix mat, tmp;
    Matrix delta(2, 1);
    delta.set(0,0)=error[0][0];
    delta.set(1,0)=error[0][1];
    for(int i=m_layers.size()-2;i>layer_index;--i){
        // TO DO: initialize tmp
        // tmp=Matrix::transpose(m_layers[i].outputs());
        delta=(Matrix::transpose(m_layers[i].weights())*delta);//.mulew(tmp);
    }
    // TO DO: initialize tmp
    tmp=m_layers[layer_index].outputs();
    m_layers[layer_index].weights()-=(delta*tmp).mul(m_lr);
}

void ANN::back_propagate(const Matrix& error){
    for(uint i=0;i<m_layers.size()-1;++i){
        back_propagate(error, i);
    }
}