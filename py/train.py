import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np

import torchmetrics.functional 

import glob
import os.path


class SequenceDataset(torch.utils.data.Dataset):

    n = 100_000

    @staticmethod
    def get_files( path ) : return sorted(glob.glob( os.path.join( path, "torch*.pt" ) ))

    def __init__(self, path='../data'):
        files = self.get_files( path )[0:10]
        self.files = files[0:-1]
        self.test_file = files[-1]
        #self.n = 10_954_278

    def __len__(self):
        return len(self.files)

    def __getitem__(self, idx):
        return self.get_file(self.files[idx])

    def get_file(self, fname ) : 
        d = torch.load( fname , weights_only = True)
        assert d.size(1)==30 
        x = d[:self.n,0:25].clone() 
        y = d[:self.n,  25].clone().unsqueeze(1) 
        return x, y



class BaseSequenceModel(nn.Module):
    def __init__(self):
        super(BaseSequenceModel, self).__init__()

    def forward(self, x):
        raise NotImplementedError("Subclasses should implement this!")

class LSTMSequenceModel(BaseSequenceModel):
    def __init__(self, input_size=4, hidden_size=16):
        super(LSTMSequenceModel, self).__init__()
        self.rnn = nn.LSTM(input_size, hidden_size,num_layers=2 , batch_first=True)
        self.fc = nn.Linear(hidden_size, 1)  # Outputs a_i and b_i

    def forward(self, x):
        output, _ = self.rnn(x)
        y = self.fc(output)
        return y  

class RNNSequenceModel(BaseSequenceModel):
    def __init__(self, input_size=4, hidden_size=16):
        super(RNNSequenceModel, self).__init__()
        self.rnn = nn.RNN(input_size, hidden_size, batch_first=True)
        self.fc = nn.Linear(hidden_size, 2)

    def forward(self, x_input, x_history):
        seq_input = torch.cat([x_input, x_history], dim=2)
        output, _ = self.rnn(seq_input)
        a_b = self.fc(output)
        return a_b

class GRUSequenceModel(BaseSequenceModel):
    def __init__(self, input_size=4, hidden_size=16):
        super(GRUSequenceModel, self).__init__()
        self.rnn = nn.GRU(input_size, hidden_size, batch_first=True)
        self.fc = nn.Linear(hidden_size, 2)

    def forward(self, x_input, x_history):
        seq_input = torch.cat([x_input, x_history], dim=2)
        output, _ = self.rnn(seq_input)
        a_b = self.fc(output)
        return a_b

class CNNSequenceModel(BaseSequenceModel):
    def __init__(self, input_size=4, hidden_size=16, kernel_size=3):
        super(CNNSequenceModel, self).__init__()
        self.conv1 = nn.Conv1d(in_channels=input_size, out_channels=hidden_size, kernel_size=kernel_size, padding=kernel_size//2)
        self.relu = nn.ReLU()
        self.conv2 = nn.Conv1d(in_channels=hidden_size, out_channels=2, kernel_size=kernel_size, padding=kernel_size//2)

    def forward(self, x_input, x_history):
        # Concatenate x_input and x_history: [batch_size, seq_length, input_size]
        seq_input = torch.cat([x_input, x_history], dim=2)  # [batch_size, seq_length, input_size]
        # Permute to match Conv1d input shape: [batch_size, input_size, seq_length]
        seq_input = seq_input.permute(0, 2, 1)
        out = self.conv1(seq_input)
        out = self.relu(out)
        out = self.conv2(out)
        # Permute back to [batch_size, seq_length, 2]
        out = out.permute(0, 2, 1)
        return out  # Shape: [batch_size, seq_length, 2]

class TransformerSequenceModel(BaseSequenceModel):
    def __init__(self, input_size=4, hidden_size=16, num_layers=2, nhead=4):
        super(TransformerSequenceModel, self).__init__()
        self.input_linear = nn.Linear(input_size, hidden_size)
        self.positional_encoding = PositionalEncoding(hidden_size)
        encoder_layer = nn.TransformerEncoderLayer(d_model=hidden_size, nhead=nhead,batch_first=True)
        self.transformer_encoder = nn.TransformerEncoder(encoder_layer, num_layers=num_layers)
        self.output_linear = nn.Linear(hidden_size, 1)  

    def forward(self, seq_input):
        # Transform to [seq_length, batch_size, input_size]
        seq_input = seq_input.permute(1, 0, 2)
        # Pass through input linear layer
        seq_input = self.input_linear(seq_input)
        # Apply positional encoding
        seq_input = self.positional_encoding(seq_input)
        # Pass through Transformer encoder
        output = self.transformer_encoder(seq_input)
        # Pass through output linear layer
        y = self.output_linear(output)
        # Transform back to [batch_size, seq_length, 2]
        y = y.permute(1, 0, 2)
        return y

class PositionalEncoding(nn.Module):
    def __init__(self, d_model, max_len=SequenceDataset.n):
        super(PositionalEncoding, self).__init__()
        # Compute the positional encodings once in log space.
        pe = torch.zeros(max_len, d_model)  # [max_len, d_model]
        position = torch.arange(0, max_len, dtype=torch.float).unsqueeze(1)  # [max_len, 1]
        div_term = torch.exp(torch.arange(0, d_model, 2).float() * (-np.log(10000.0) / d_model))  # [d_model/2]
        pe[:, 0::2] = torch.sin(position * div_term)  # Even indices
        pe[:, 1::2] = torch.cos(position * div_term)  # Odd indices
        pe = pe.unsqueeze(1)  # [max_len, 1, d_model]
        self.register_buffer('pe', pe)

    def forward(self, x):
        # x: [seq_len, batch_size, d_model]
        x = x + self.pe[:x.size(0), :]
        return x

def train_model(model, dataloader, criterion, optimizer, num_epochs=10):
    model.train()
    for epoch in range(num_epochs):
        epoch_loss = 0.0
        for x, y_true in dataloader:
            optimizer.zero_grad()
            y_pred = model(x)
            print("train y_pred shape:", y_pred.shape)
            loss = criterion(y_pred, y_true)
            loss.backward()
            optimizer.step()
            epoch_loss += loss.item()
        avg_loss = epoch_loss / len(dataloader)
        print(f'Epoch [{epoch+1}/{num_epochs}], Loss: {avg_loss:.4f}')
        if avg_loss < 0.1 : break
    return model

def main(model_type='LSTM', seq_length=10, batch_size=32, num_epochs=500, learning_rate=0.001):
    # Create dataset and dataloader
    dataset = SequenceDataset()
    dataloader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, shuffle=True)

    # Choose the model based on the argument
    if model_type == 'LSTM':
        model = LSTMSequenceModel(input_size=25, hidden_size=16)
    elif model_type == 'RNN':
        model = RNNSequenceModel(input_size=25, hidden_size=16)
    elif model_type == 'GRU':
        model = GRUSequenceModel(input_size=25, hidden_size=16)
    elif model_type == 'CNN':
        model = CNNSequenceModel(input_size=25, hidden_size=16, kernel_size=3)
    elif model_type == 'Transformer':
        model = TransformerSequenceModel(input_size=25, hidden_size=16, num_layers=2, nhead=4)
    else:
        raise ValueError("Invalid model_type. Choose from 'LSTM', 'RNN', 'GRU', 'CNN', or 'Transformer'.")

    # Initialize loss function and optimizer
    #criterion = nn.MSELoss()
    def one_minus_r2( predictions, answers ) : 
        return 1-torchmetrics.functional.r2_score( predictions.squeeze(), answers.squeeze() )
    criterion = one_minus_r2
    optimizer = optim.Adam(model.parameters(), lr=learning_rate)

    print(f"\nTraining using {model_type} model:")
    # Train the model
    _ = train_model(model, dataloader, criterion, optimizer, num_epochs)

    # Testing the model on a new sequence
    model.eval()
    with torch.no_grad():
        # Generate a test sample
        x, y_true = dataset.get_file( dataset.test_file )
        x = x.unsqueeze(0)  # Add batch dimension
        y_true = y_true.unsqueeze(0)

        # Predict a_i and b_i
        y_pred = model(x)
        loss = criterion(y_pred, y_true)

        # Print results
        print("\nSample Predictions:", loss)

if __name__ == '__main__':
    # You can change the model_type to 'LSTM', 'RNN', 'GRU', 'CNN', or 'Transformer'
    main(model_type='Transformer')
