import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np

class SequenceDataset(torch.utils.data.Dataset):
    def __init__(self, seq_length=10, total_samples=1000):
        self.seq_length = seq_length
        self.total_samples = total_samples

    def __len__(self):
        return self.total_samples

    def __getitem__(self, idx):
        # Generate random x sequence
        x_seq = torch.rand(self.seq_length + 3) * 20 - 10  # Extra 3 for history
        y_seq = torch.zeros(self.seq_length)

        # Compute y_i = 2x_i + (x_{i-1} + x_{i-2} + x_{i-3}) / 3
        for i in range(3, self.seq_length + 3):
            avg_prev_x = torch.mean(x_seq[i - 3:i])
            y_seq[i - 3] = 2 * x_seq[i] + avg_prev_x + torch.randn(1) * 2  # Add noise

        # Prepare input and target sequences
        x_input = x_seq[3:].unsqueeze(1)  # x_i
        x_history = torch.stack([x_seq[2:-1], x_seq[1:-2], x_seq[0:-3]], dim=1)  # x_{i-1}, x_{i-2}, x_{i-3}

        return x_input, x_history, y_seq.unsqueeze(1)

class BaseSequenceModel(nn.Module):
    def __init__(self):
        super(BaseSequenceModel, self).__init__()

    def forward(self, x_input, x_history):
        raise NotImplementedError("Subclasses should implement this!")

class LSTMSequenceModel(BaseSequenceModel):
    def __init__(self, input_size=4, hidden_size=16):
        super(LSTMSequenceModel, self).__init__()
        self.rnn = nn.LSTM(input_size, hidden_size, batch_first=True)
        self.fc = nn.Linear(hidden_size, 2)  # Outputs a_i and b_i

    def forward(self, x_input, x_history):
        seq_input = torch.cat([x_input, x_history], dim=2)
        output, _ = self.rnn(seq_input)
        a_b = self.fc(output)
        return a_b  # Shape: [batch_size, seq_length, 2]

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
        encoder_layer = nn.TransformerEncoderLayer(d_model=hidden_size, nhead=nhead)
        self.transformer_encoder = nn.TransformerEncoder(encoder_layer, num_layers=num_layers)
        self.output_linear = nn.Linear(hidden_size, 2)  # Outputs a_i and b_i

    def forward(self, x_input, x_history):
        # Concatenate x_input and x_history: [batch_size, seq_length, input_size]
        seq_input = torch.cat([x_input, x_history], dim=2)  # [batch_size, seq_length, input_size]
        # Transform to [seq_length, batch_size, input_size]
        seq_input = seq_input.permute(1, 0, 2)
        # Pass through input linear layer
        seq_input = self.input_linear(seq_input)
        # Apply positional encoding
        seq_input = self.positional_encoding(seq_input)
        # Pass through Transformer encoder
        output = self.transformer_encoder(seq_input)
        # Pass through output linear layer
        a_b = self.output_linear(output)
        # Transform back to [batch_size, seq_length, 2]
        a_b = a_b.permute(1, 0, 2)
        return a_b

class PositionalEncoding(nn.Module):
    def __init__(self, d_model, max_len=5000):
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
        for x_input, x_history, y_true in dataloader:
            optimizer.zero_grad()
            a_b = model(x_input, x_history)
            a_i = a_b[:, :, 0]
            b_i = a_b[:, :, 1]
            y_pred = a_i * x_input.squeeze(2) + b_i
            loss = criterion(y_pred, y_true.squeeze(2))
            loss.backward()
            optimizer.step()
            epoch_loss += loss.item()
        avg_loss = epoch_loss / len(dataloader)
        print(f'Epoch [{epoch+1}/{num_epochs}], Loss: {avg_loss:.4f}')
    return model

def main(model_type='LSTM', seq_length=10, batch_size=32, num_epochs=10, learning_rate=0.001):
    # Create dataset and dataloader
    dataset = SequenceDataset(seq_length=seq_length, total_samples=1000)
    dataloader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, shuffle=True)

    # Choose the model based on the argument
    if model_type == 'LSTM':
        model = LSTMSequenceModel(input_size=4, hidden_size=16)
    elif model_type == 'RNN':
        model = RNNSequenceModel(input_size=4, hidden_size=16)
    elif model_type == 'GRU':
        model = GRUSequenceModel(input_size=4, hidden_size=16)
    elif model_type == 'CNN':
        model = CNNSequenceModel(input_size=4, hidden_size=16, kernel_size=3)
    elif model_type == 'Transformer':
        model = TransformerSequenceModel(input_size=4, hidden_size=16, num_layers=2, nhead=4)
    else:
        raise ValueError("Invalid model_type. Choose from 'LSTM', 'RNN', 'GRU', 'CNN', or 'Transformer'.")

    # Initialize loss function and optimizer
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=learning_rate)

    print(f"\nTraining using {model_type} model:")
    # Train the model
    trained_model = train_model(model, dataloader, criterion, optimizer, num_epochs)

    # Testing the model on a new sequence
    model.eval()
    with torch.no_grad():
        # Generate a test sample
        x_input, x_history, y_true = dataset[0]
        x_input = x_input.unsqueeze(0)  # Add batch dimension
        x_history = x_history.unsqueeze(0)
        y_true = y_true.unsqueeze(0)

        # Predict a_i and b_i
        a_b = model(x_input, x_history)
        a_i = a_b[:, :, 0]
        b_i = a_b[:, :, 1]
        y_pred = a_i * x_input.squeeze(2) + b_i

        # Print results
        print("\nSample Predictions:")
        for i in range(seq_length):
            print(f"Time Step {i+1}: a_i = {a_i[0, i]:.4f}, b_i = {b_i[0, i]:.4f}, "
                  f"y_true = {y_true[0, i].item():.4f}, y_pred = {y_pred[0, i].item():.4f}")

if __name__ == '__main__':
    # You can change the model_type to 'LSTM', 'RNN', 'GRU', 'CNN', or 'Transformer'
    main(model_type='Transformer')
